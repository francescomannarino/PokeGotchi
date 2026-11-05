#!/usr/bin/env python3
"""Test degli strumenti dell'officina (tools/) e dei file generati.

  python3 test/test_tools.py
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TOOLS = os.path.join(ROOT, 'tools')
sys.path.insert(0, TOOLS)


def file_della_ui():
    """Lo sketch e i file della UI divisi da esso (ui_*.h/.cpp, console.cpp)."""
    nomi = sorted(n for n in os.listdir(ROOT)
                  if re.fullmatch(r'ui_\w+\.(h|cpp)', n) or n == 'console.cpp')
    return ['PokeGotchi.ino'] + nomi


def leggi_sorgente(nome):
    with open(os.path.join(ROOT, nome), encoding='utf-8') as fh:
        return fh.read()


class TestToolsCompile(unittest.TestCase):
    def test_tutti_gli_script_compilano(self):
        scripts = sorted(f for f in os.listdir(TOOLS) if f.endswith('.py'))
        self.assertTrue(scripts, 'no se encontro ningun script en tools/')
        for name in scripts:
            with self.subTest(script=name):
                with open(os.path.join(TOOLS, name), encoding='utf-8') as fh:
                    src = fh.read()
                try:
                    compile(src, name, 'exec')
                except SyntaxError as e:
                    self.fail(f'{name} no compila: {e}')

    def test_gli_script_eseguibili_dichiarano_lo_shebang(self):
        for name in sorted(f for f in os.listdir(TOOLS) if f.endswith('.py')):
            path = os.path.join(TOOLS, name)
            if not os.access(path, os.X_OK):
                continue
            with open(path) as fh:
                self.assertTrue(fh.readline().startswith('#!'),
                                f'{name} es ejecutable pero no tiene shebang')


class TestDexData(unittest.TestCase):
    """Invarianti della fonte dei dati del Pokédex."""

    @classmethod
    def setUpClass(cls):
        from dex_data import DEX, TYPE_ACCENTS, CLASSIC, RARE, LEGENDARY, SLUGS
        from dex_stats import BASE_STATS
        from dex_names import LOCAL_NAMES
        from dex_move_names import MOVE_NAMES
        cls.DEX, cls.ACCENTS, cls.CLASSIC = DEX, TYPE_ACCENTS, CLASSIC
        cls.RARE, cls.LEGENDARY, cls.SLUGS = RARE, LEGENDARY, SLUGS
        cls.STATS, cls.NAMES = BASE_STATS, LOCAL_NAMES
        cls.byNum = {row[0]: row for row in DEX}

    def test_ci_sono_le_151_specie_senza_buchi_ne_doppioni(self):
        nums = [row[0] for row in self.DEX]
        self.assertEqual(len(nums), 151)
        self.assertEqual(sorted(nums), list(range(1, 152)))

    def test_gli_slug_sono_unici_e_minuscoli(self):
        slugs = [row[1] for row in self.DEX]
        self.assertEqual(len(set(slugs)), len(slugs), 'hay slugs repetidos')
        for s in slugs:
            self.assertEqual(s, s.lower(), f'slug con mayusculas: {s}')
            self.assertTrue(s.isascii(), f'slug no ASCII: {s}')

    def test_i_nomi_a_schermo_sono_ascii_maiuscoli(self):
        # il font GFX del firmware non ha accenti né minuscole
        for num, slug, display, *_ in self.DEX:
            self.assertTrue(display.isascii(), f'{num} {display} no es ASCII')
            self.assertEqual(display, display.upper(), f'{num} {display} lleva minusculas')
            self.assertLessEqual(len(display), 12, f'{num} {display} no cabe en pantalla')

    def test_le_evoluzioni_puntano_a_specie_reali(self):
        for num, slug, display, tipo, evo, lvl in self.DEX:
            if evo:
                self.assertIn(evo, self.byNum, f'{display} evoluciona a {evo}, que no existe')
                self.assertNotEqual(evo, num, f'{display} evoluciona a si mismo')
                self.assertGreater(lvl, 0, f'{display} evoluciona sin nivel')
            else:
                self.assertEqual(lvl, 0, f'{display} es forma final pero tiene nivel {lvl}')

    def test_nessuna_catena_evolutiva_fa_un_ciclo(self):
        for num in self.byNum:
            seen, cur = set(), num
            while cur and self.byNum[cur][4]:
                self.assertNotIn(cur, seen, f'bucle evolutivo desde {num}')
                seen.add(cur)
                cur = self.byNum[cur][4]
                self.assertLessEqual(len(seen), 5, f'cadena demasiado larga desde {num}')

    def test_tutti_i_tipi_hanno_un_colore(self):
        for num, slug, display, tipo, *_ in self.DEX:
            self.assertIn(tipo, self.ACCENTS, f'{display}: tipo "{tipo}" sin color')

    def test_i_colori_sono_hex_a_6_cifre(self):
        for tipo, col in self.ACCENTS.items():
            self.assertRegex(col, r'^#[0-9a-fA-F]{6}$', f'color raro en {tipo}: {col}')

    def test_le_rarita_puntano_a_forme_base(self):
        evolute = {row[4] for row in self.DEX if row[4]}
        for num in sorted(self.RARE | self.LEGENDARY):
            self.assertIn(num, self.byNum, f'rareza para una especie inexistente: {num}')
            self.assertNotIn(num, evolute,
                             f'{self.byNum[num][2]} solo sale por evolucion: la rareza no le sirve')
        self.assertFalse(self.RARE & self.LEGENDARY, 'una especie no puede ser rara y legendaria')

    def test_gli_iniziali_classici_esistono(self):
        evolute = {row[4] for row in self.DEX if row[4]}
        for num in self.CLASSIC:
            self.assertIn(num, self.byNum, f'inicial inexistente: {num}')
            self.assertNotIn(num, evolute, f'{self.byNum[num][2]} no puede ser inicial')

    def test_ci_sono_le_stats_base_delle_151(self):
        for num in self.byNum:
            self.assertIn(num, self.STATS, f'faltan stats de {self.byNum[num][2]}')
            hp, atk, dfn, spe = self.STATS[num]
            for v in (hp, atk, dfn, spe):
                self.assertTrue(0 < v <= 255, f'stat fuera de rango en {self.byNum[num][2]}: {v}')

    def test_i_nomi_tradotti_coincidono_con_la_fonte(self):
        # I latini si disegnano con il font CP437 (un byte per carattere, senza
        # minuscole nei nomi); il giapponese e il coreano con un font U8g2 e vanno in
        # UTF-8, quindi né sono ASCII né hanno maiuscole da verificare.
        LATINE, CJK = ('fr', 'de'), ('ja', 'ko')
        for num, langs in self.NAMES.items():
            self.assertIn(num, self.byNum, f'nombre traducido de una especie inexistente: {num}')
            for lang, name in langs.items():
                self.assertIn(lang, LATINE + CJK, f'idioma inesperado: {lang}')
                self.assertTrue(name, f'{num} {lang} vacio')
                if lang in LATINE:
                    self.assertTrue(name.isascii(), f'{num} {lang} no es ASCII: {name}')
                    self.assertEqual(name, name.upper(), f'{num} {lang} lleva minusculas: {name}')
                    self.assertLessEqual(len(name), 12, f'{num} {lang} no cabe en pantalla: {name}')
                else:
                    # il limite utile sono i CARATTERI, non i byte: in UTF-8 ogni kana o
                    # sillaba hangul occupa 3
                    self.assertLessEqual(len(name), 8, f'{num} {lang} no cabe en pantalla: {name}')


class TestGeneratedFiles(unittest.TestCase):
    """dex.h e moves.h sono generati: se non coincidono con il loro generatore, qualcuno li
    ha modificati a mano."""

    def test_gen_dex_riproduce_dex_h_e_moves_h(self):
        tmp = tempfile.mkdtemp(prefix='pokegotchi-gen-')
        try:
            shutil.copytree(TOOLS, os.path.join(tmp, 'tools'))
            r = subprocess.run([sys.executable, os.path.join(tmp, 'tools', 'gen_dex.py')],
                               capture_output=True, text=True)
            self.assertEqual(r.returncode, 0, f'gen_dex.py fallo:\n{r.stderr}')
            for nome in ('dex.h', 'moves.h'):
                generated = os.path.join(tmp, nome)
                self.assertTrue(os.path.exists(generated), f'gen_dex.py no escribio {nome}')
                with open(generated, encoding='utf-8') as a, \
                        open(os.path.join(ROOT, nome), encoding='utf-8') as b:
                    self.assertEqual(a.read(), b.read(),
                                     f'{nome} no coincide con lo que genera tools/gen_dex.py '
                                     '(vuelve a generarlo en vez de editarlo a mano)')
        finally:
            shutil.rmtree(tmp, ignore_errors=True)

    def test_gen_musica_riproduce_musica_dati_h(self):
        tmp = tempfile.mkdtemp(prefix='pokegotchi-gen-')
        try:
            shutil.copytree(TOOLS, os.path.join(tmp, 'tools'))
            r = subprocess.run([sys.executable, os.path.join(tmp, 'tools', 'gen_musica.py')],
                               capture_output=True, text=True)
            self.assertEqual(r.returncode, 0, f'gen_musica.py fallito:\n{r.stderr}')
            with open(os.path.join(tmp, 'musica_dati.h'), encoding='utf-8') as a, \
                    open(os.path.join(ROOT, 'musica_dati.h'), encoding='utf-8') as b:
                self.assertEqual(a.read(), b.read(),
                                 'musica_dati.h non coincide con tools/gen_musica.py '
                                 '(rigeneralo invece di modificarlo a mano)')
        finally:
            shutil.rmtree(tmp, ignore_errors=True)

    def test_i_file_generati_dichiarano_di_esserlo(self):
        for name in ('dex.h', 'species.h', 'moves.h', 'musica_dati.h'):
            with open(os.path.join(ROOT, name)) as fh:
                head = fh.read(400)
            self.assertIn('GENERATO', head, f'{name} deberia decir que esta generado')


class TestDiagrammi(unittest.TestCase):
    """Gli SVG di docs/img/comandi/ sono generati: devono coincidere con make_diagrams.py."""

    CARTELLA = os.path.join(ROOT, 'docs', 'img', 'comandi')

    def test_gli_svg_coincidono_con_il_generatore(self):
        tmp = tempfile.mkdtemp(prefix='pokegotchi-diagrammi-')
        try:
            r = subprocess.run([sys.executable, os.path.join(TOOLS, 'make_diagrams.py'),
                                '--out', tmp], capture_output=True, text=True)
            self.assertEqual(r.returncode, 0, f'make_diagrams.py fallito:\n{r.stderr}')
            generati = sorted(os.listdir(tmp))
            self.assertEqual(generati, sorted(os.listdir(self.CARTELLA)),
                             'docs/img/comandi/ ha file in più o in meno del generatore')
            for nome in generati:
                with open(os.path.join(tmp, nome), encoding='utf-8') as a, \
                        open(os.path.join(self.CARTELLA, nome), encoding='utf-8') as b:
                    self.assertEqual(a.read(), b.read(),
                                     f'{nome} non coincide con tools/make_diagrams.py '
                                     '(rigeneralo invece di modificarlo a mano)')
        finally:
            shutil.rmtree(tmp, ignore_errors=True)

    def test_gli_svg_sono_statici(self):
        # GitHub li mostra come <img>: niente script, CSS né risorse esterne
        for nome in os.listdir(self.CARTELLA):
            with open(os.path.join(self.CARTELLA, nome), encoding='utf-8') as fh:
                svg = fh.read()
            for vietato in ('<script', '<style', 'href=', '@import', 'url(http', 'onload='):
                self.assertNotIn(vietato, svg, f'{nome} contiene {vietato}')

    def test_comandi_md_usa_tutti_i_diagrammi(self):
        import make_diagrams
        with open(os.path.join(ROOT, 'COMANDI.md'), encoding='utf-8') as fh:
            comandi = fh.read()
        self.assertNotIn('<!-- diagramma:', comandi, 'resta un segnaposto in COMANDI.md')
        for nome in make_diagrams.SCHEDE:
            self.assertIn(make_diagrams.picture(nome), comandi,
                          f'COMANDI.md non ha il blocco <picture> aggiornato di {nome}')


class TestSketchSanity(unittest.TestCase):
    """Controlli economici sullo sketch e sui file della UI che non richiedono di compilarlo."""

    @classmethod
    def setUpClass(cls):
        cls.file = {nome: leggi_sorgente(nome) for nome in file_della_ui()}

    def test_lo_sketch_non_ha_caratteri_non_ascii_nei_letterali_a_schermo(self):
        # i commenti possono contenere ciò che vogliono; i letterali no: il font GFX è
        # ASCII e un accento esce come spazzatura
        for nome, testo in self.file.items():
            for lineno, line in enumerate(testo.splitlines(), 1):
                code = line.split('//')[0]
                for chunk in code.split('"')[1::2]:
                    self.assertTrue(chunk.isascii(),
                                    f'{nome}:{lineno}: literal no ASCII: {chunk!r}')

    def test_le_graffe_sono_bilanciate(self):
        for nome, testo in self.file.items():
            self.assertEqual(testo.count('{'), testo.count('}'),
                             f'llaves descompensadas en {nome}')

    def test_la_ui_e_divisa_nei_file_attesi(self):
        nomi = set(self.file)
        for atteso in ('PokeGotchi.ino', 'ui_common.h', 'ui_common.cpp', 'ui_main.cpp',
                       'console.cpp'):
            self.assertIn(atteso, nomi)


class TestBufferDiTesto(unittest.TestCase):
    """Ogni snprintf(buf, sizeof(buf), T(S_x), ...) sta nel suo buffer in TUTTE le lingue.

    I buffer furono dimensionati quando ogni stringa era di un byte per
    carattere. In UTF-8 un kana o una sillaba hangul ne occupano 3, e snprintf taglia
    in silenzio: a volte si perdono cifre ("きろく 2" invece di "きろく 219") e a
    volte taglia a metà di un carattere, che il font non sa più disegnare. Non lo
    segnala né il compilatore né lo schermo, quindi si verifica qui con il
    caso peggiore di ogni argomento secondo il suo tipo:

      %u / %d  -> 5 cifre (tutti gli argomenti sono a 16 bit o meno)
      %lu      -> 10 cifre
      %s       -> ciò che dichiara ARGS_S; un %s non dichiarato fa fallire il
                  test, perché chi lo aggiunge pensi a quanto può occupare
    """

    ARGS_S = {  # cosa può arrivare per ogni %s, in ordine
        'S_NAME_FMT': ('stella', 'nome_o_soprannome'),
        'S_RELEASE_FMT': ('nome',),
        'S_INFO_FMT': ('bacca',),
        'S_FAREWELL_BTN': ('nome_o_soprannome',),
        'S_RUNAWAY_BTN': ('nome_o_soprannome',),
        # battaglia: un lato è il pet (soprannome) o il selvatico (nome della specie)
        'S_BTL_APPEARS': ('nome',),
        'S_BTL_USES': ('nome_o_soprannome', 'mossa'),
        'S_BTL_STAT_UP': ('nome_o_soprannome', 'stat'),
        'S_BTL_STAT_DOWN': ('nome_o_soprannome', 'stat'),
        'S_BTL_TRANSFORM': ('nome_o_soprannome', 'nome'),
        **{f'S_BTL_{k}': ('nome_o_soprannome',) for k in (
            'GO', 'STRUGGLE', 'RECHARGE', 'RECOIL', 'HEAL', 'FAINT', 'CHARGE', 'PARA', 'BURN',
            'POISON', 'TOXIC', 'SLEEP', 'FROZEN', 'CONFUSED', 'ASLEEP', 'WAKE', 'THAW',
            'CANT_MOVE', 'UNCONFUSED', 'FLINCH', 'PSN_DMG', 'BRN_DMG', 'SEED_DMG', 'SEEDED',
            'RAGE', 'PROTECTED', 'MIST', 'FOCUS', 'BARRIER', 'CONVERT', 'REWARD')},
        # collezione: il pet attivo o un ritratto (soprannome o nome della specie)
        'S_COL_TAKE_Q': ('nome_o_soprannome',),
        'S_COL_FREE_Q': ('nome_o_soprannome',),
        'S_COL_KEEP_Q': ('nome_o_soprannome',),
        # battaglia in collegamento: il nome dell'allenatore (LINK_NOME_LEN) e i Pokémon
        # delle squadre (soprannome o nome della specie)
        'S_LKB_WANTS': ('allenatore',),
        'S_LKB_PROPOSES': ('allenatore',),
        'S_LKB_WAITING': ('allenatore',),
        'S_LKB_SENDS': ('allenatore', 'nome_o_soprannome'),
        'S_LKB_BACK': ('nome_o_soprannome',),
        'S_LKB_WITHDRAW': ('allenatore', 'nome_o_soprannome'),
        'S_LKB_GAVE_UP': ('allenatore',),
        'S_LKB_WANTS_REMATCH': ('allenatore',),
        # scambio: l'allenatore dell'altro e i due pet (soprannome o nome della specie)
        'S_LKS_CONFIRM_Q': ('nome_o_soprannome', 'nome_o_soprannome'),
        'S_LKS_THEY_OK': ('allenatore',),
        'S_LKS_GOT': ('nome_o_soprannome',),
        'S_LKS_EVOLVES': ('nome_o_soprannome',),
        'S_LKS_SUSPENDED': ('allenatore',),
        # aggiornamento: una versione (OtaManifest::versione, 15 byte) e il nome di una rete
        # WiFi (32 byte, già ridotto all'ASCII stampabile)
        'S_AG_VERSIONE': ('versione',),
        'S_AG_NUOVA': ('versione',),
        'S_AG_RETE': ('rete',),
    }
    STAT_BATTAGLIA = ('S_BTL_ST_ATK', 'S_BTL_ST_DEF', 'S_BTL_ST_SPE', 'S_BTL_ST_SPC',
                      'S_BTL_ST_ACC', 'S_BTL_ST_EVA')
    CIFRE = {'u': 5, 'd': 5, 'lu': 10}
    SPECIFICATORE = re.compile(r'%0?\d*(lu|u|d|s)')

    @classmethod
    def setUpClass(cls):
        import test_i18n_formats as fmt
        from dex_data import DEX
        from dex_names import LOCAL_NAMES
        from dex_move_names import MOVE_NAMES

        def leggi(nome):
            with open(os.path.join(ROOT, nome), encoding='utf-8') as fh:
                return fh.read()

        cls.LANGS = fmt.LANGS
        corpo = re.search(r'enum\s+StrId[^{]*\{(.*?)\}', leggi('i18n.h'), re.S).group(1)
        ids = re.findall(r'\b(S_[A-Z0-9_]+)\b', corpo)
        righe = fmt.extract_table(leggi('i18n.cpp'), 'STRINGS', len(cls.LANGS))
        cls.LIT = {sid: [riga[k] for riga in righe] for k, sid in enumerate(ids)}
        cls.file = {nome: leggi(nome).splitlines() for nome in file_della_ui()}
        cls.SOPRANNOME = int(re.search(r'char\s+nick\[(\d+)\]', leggi('pet.h')).group(1)) - 1
        cls.ALLENATORE = int(re.search(r'#define\s+LINK_NOME_LEN\s+(\d+)', leggi('link.h')).group(1))
        base = max(len(row[2].encode()) for row in DEX)
        cls.NOME = {}
        for lang in cls.LANGS:
            propri = [n[lang.lower()] for n in LOCAL_NAMES.values() if lang.lower() in n]
            cls.NOME[lang] = max([base] + [len(n.encode('utf-8')) for n in propri])
        # nome di mossa più lungo per lingua (il PT ricade sull'inglese)
        cls.MOSSA = {lang: max(len(n['en' if lang == 'PT' else lang.lower()].encode('utf-8'))
                               for n in MOVE_NAMES.values()) for lang in cls.LANGS}

    @staticmethod
    def bytes_literal(literal):
        """Byte che occupa in memoria un letterale C scritto come nel sorgente."""
        corpo = literal[1:-1].encode('utf-8')
        n = i = 0
        while i < len(corpo):
            if corpo[i] == 0x5C:  # backslash: \ooo ottale, oppure \n \" \\ ...
                j = i + 1
                while j < len(corpo) and j < i + 4 and 0x30 <= corpo[j] <= 0x37:
                    j += 1
                i = j if j > i + 1 else i + 2
            else:
                i += 1
            n += 1
        return n

    def caso_peggiore(self, sid, li):
        lang, lit = self.LANGS[li], self.LIT[sid][li]
        total = self.bytes_literal(lit) + 1  # + terminatore
        in_sospeso = list(self.ARGS_S.get(sid, ()))
        for m in self.SPECIFICATORE.finditer(lit):
            total -= len(m.group(0))
            if m.group(1) != 's':
                total += self.CIFRE[m.group(1)]
                continue
            self.assertTrue(in_sospeso, f'{sid}: tiene un %s sin declarar en ARGS_S')
            arg = in_sospeso.pop(0)
            if arg == 'stella':
                total += 1
            elif arg == 'nome':
                total += self.NOME[lang]
            elif arg == 'nome_o_soprannome':
                total += max(self.NOME[lang], self.SOPRANNOME)
            elif arg == 'allenatore':
                total += self.ALLENATORE
            elif arg == 'mossa':
                total += self.MOSSA[lang]
            elif arg == 'stat':
                total += max(self.bytes_literal(self.LIT[st][li]) for st in self.STAT_BATTAGLIA)
            elif arg == 'versione':
                total += 15
            elif arg == 'rete':
                total += 32
            elif arg == 'bacca':
                total += max(self.bytes_literal(self.LIT[b][li]) for b in
                             ('S_BERRY_UNK', 'S_BERRY_RED', 'S_BERRY_BLUE', 'S_BERRY_GREEN'))
        return total

    def test_ogni_testo_formattato_sta_nel_buffer_in_tutte_le_lingue(self):
        chiamata = re.compile(r'snprintf\((\w+),\s*sizeof\(\1\),\s*T\((S_[A-Z0-9_]+)\)')
        verificate = 0
        for nome, righe in self.file.items():
            for n, riga in enumerate(righe, 1):
                m = chiamata.search(riga.split('//')[0])
                if not m:
                    continue
                var, sid = m.groups()
                dim = next((int(d.group(1)) for k in range(n - 2, -1, -1)
                            for d in [re.search(r'\bchar\s+' + var + r'\s*\[(\d+)\]', righe[k])]
                            if d), None)
                self.assertIsNotNone(dim, f'{nome}:{n}: no encuentro la declaracion de {var}')
                verificate += 1
                for li, lang in enumerate(self.LANGS):
                    with self.subTest(file=nome, riga=n, id=sid, lingua=lang):
                        peggiore = self.caso_peggiore(sid, li)
                        self.assertLessEqual(
                            peggiore, dim,
                            f'{nome}:{n}: {var}[{dim}] con {sid} en {lang} necesita {peggiore} bytes')
        self.assertGreater(verificate, 10, 'el patron de snprintf ya no encuentra las chiamatas')

class TestSalvataggi(unittest.TestCase):
    """tools/salvataggi.py decodifica il .pgsave di esempio scritto da test_save.cpp."""

    ESEMPIO = os.path.join(ROOT, 'test', 'dati', 'esempio.pgsave')

    def info(self, percorso):
        return subprocess.run([sys.executable, os.path.join(TOOLS, 'salvataggi.py'), 'info',
                               percorso], capture_output=True, text=True)

    def copia_modificata(self, cambia):
        with open(self.ESEMPIO, 'rb') as fh:
            dati = bytearray(fh.read())
        dati = cambia(dati)
        fd, percorso = tempfile.mkstemp(suffix='.pgsave')
        with os.fdopen(fd, 'wb') as fh:
            fh.write(dati)
        self.addCleanup(os.remove, percorso)
        return percorso

    def test_info_decodifica_intestazione_riepilogo_e_chiavi(self):
        r = self.info(self.ESEMPIO)
        self.assertEqual(r.returncode, 0, r.stderr)
        for atteso in ('formato      1  schema 2', 'firmware 1.17', 'motivo       manuale',
                       'ok', '25 (PIKACHU)', 'nome         SPARKY', '[pokegotchi]',
                       'stato del Pokémon: versione 5, CRC ok', 'collezione: 1 Pokémon',
                       '0: 26 (RAICHU) livello 2, nome RAICHU'):
            self.assertIn(atteso, r.stdout)
        # scritti da preparaPet(): soprannome, allenamento, legame (nel blob "st"), lingua
        for chiave in (r'st\s+blob\s+143 byte', r'col\s+blob\s+52 byte', r"nick\s+'SPARKY'", r'trAtk\s+40',
                       r'bond\s+33', r'grottaHi\s+42\n', r'surfHi\s+7\n', r'lang\s+u8\s+3',
                       r'dexReg\s+[0-9a-f]{38}\n'):
            self.assertRegex(r.stdout, chiave)

    def test_la_decodifica_coincide_con_l_intestazione(self):
        from salvataggi import decodifica
        with open(self.ESEMPIO, 'rb') as fh:
            h = decodifica(fh.read())
        self.assertEqual(h['riepilogo']['specie'], 25)
        self.assertEqual(h['lunghezza'], os.path.getsize(self.ESEMPIO))
        chiavi = {c: v for ns, voci in h['namespace'] for _, c, v in voci}
        self.assertNotIn('dexn', chiavi)  # schema 2: lo stato è solo nel blob
        self.assertEqual(chiavi['seen'], h['epoch'])  # esportare allinea l'orologio
        pet, problema = h['pet']
        self.assertIsNone(problema)
        self.assertEqual(pet['speciesId'], 25)
        self.assertEqual(pet['nick'], 'SPARKY')
        self.assertEqual(pet['size'], len(chiavi['st']))
        (rit, problema), = h['collezione']
        self.assertIsNone(problema)
        self.assertEqual((rit['specie'], rit['nick'], rit['otId']), (26, 'RAICHU', 0))

    def test_il_layout_del_ritratto_coincide_con_ritratto_h(self):
        # tabella «off dim campo» del commento di testa di ritratto.h contro RITRATTO_CAMPI
        import struct
        from salvataggi import RITRATTO_CAMPI
        with open(os.path.join(ROOT, 'ritratto.h'), encoding='utf-8') as fh:
            testo = fh.read()
        righe = re.findall(r'^//\s+(\d+)\s+(\d+)\s+(\w+)', testo, re.M)
        attese, off = [], 0
        for nome, fmt in RITRATTO_CAMPI:
            dim = struct.calcsize('<' + fmt)
            attese.append((str(off), str(dim), nome))
            off += dim
        self.assertEqual(righe, attese)
        dim = int(re.search(r'#define RIT_BYTES (\d+)', testo).group(1))
        self.assertEqual(off, dim)

    def test_un_ritratto_rovinato_si_segnala(self):
        from salvataggi import decodifica

        def rovina_ritratto(d):
            d[d.index(b'RAICHU')] ^= 0x01  # nel ritratto; CRC del file ricalcolato
            d[-4:] = zlib.crc32(bytes(d[:-4])).to_bytes(4, 'little')
            return d
        percorso = self.copia_modificata(rovina_ritratto)
        with open(percorso, 'rb') as fh:
            (_, problema), = decodifica(fh.read())['collezione']
        self.assertEqual(problema, 'CRC del ritratto errato')
        self.assertIn('il firmware lo scarta', self.info(percorso).stdout)

    def test_il_layout_del_blob_coincide_con_petsave(self):
        # campi e dimensione di `struct PetSave` (pet.h) contro PET_SAVE_CAMPI
        import struct
        from salvataggi import PET_SAVE_CAMPI
        with open(os.path.join(ROOT, 'pet.h'), encoding='utf-8') as fh:
            testo = fh.read()
        corpo = re.search(r'struct __attribute__\(\(packed\)\) PetSave \{(.*?)\n\};',
                          testo, re.S).group(1)
        corpo = re.sub(r'//[^\n]*', '', corpo)
        tipi = {'uint8_t': 'B', 'int8_t': 'b', 'uint16_t': 'H', 'int16_t': 'h',
                'uint32_t': 'I', 'int32_t': 'i', 'char': 's'}
        campi = []
        for istr in filter(None, (x.strip() for x in corpo.split(';'))):
            tipo, resto = istr.split(None, 1)
            for decl in resto.split(','):
                m = re.match(r'\s*(\w+)\s*(?:\[(\d+)\])?', decl)
                n = m.group(2)
                fmt = tipi[tipo]
                campi.append((m.group(1), f'{n}s' if n else fmt))
        self.assertEqual(campi, PET_SAVE_CAMPI)
        dim = int(re.search(r'static_assert\(sizeof\(PetSave\) == (\d+)', testo).group(1))
        self.assertEqual(struct.calcsize('<' + ''.join(f for _, f in PET_SAVE_CAMPI)), dim)

    def test_un_blob_rovinato_si_segnala(self):
        from salvataggi import decodifica

        def rovina_blob(d):
            d[d.rindex(b'SPARKY')] ^= 0x01  # nel blob (il primo è nel riepilogo); CRC del file ricalcolato
            d[-4:] = zlib.crc32(bytes(d[:-4])).to_bytes(4, 'little')
            return d
        percorso = self.copia_modificata(rovina_blob)
        with open(percorso, 'rb') as fh:
            _, problema = decodifica(fh.read())['pet']
        self.assertEqual(problema, 'CRC del blob errato')
        self.assertIn('CRC del blob errato', self.info(percorso).stdout)

    def test_file_rovinati_rifiutati(self):
        def corrompi(d):
            d[60] ^= 0xFF
            return d
        casi = {
            'CRC': corrompi,
            'LUNGHEZZA': lambda d: d[:-1],
            'MAGIC': lambda d: b'XPSV' + d[4:],
        }
        for esito, cambia in casi.items():
            with self.subTest(esito=esito):
                r = self.info(self.copia_modificata(cambia))
                self.assertNotEqual(r.returncode, 0)
                self.assertIn(esito, r.stderr)

    def test_la_risposta_a_export_si_riconosce(self):
        import base64
        from salvataggi import estrai_salvataggio
        with open(self.ESEMPIO, 'rb') as fh:
            dati = fh.read()
        self.assertEqual(estrai_salvataggio(base64.b64encode(dati).decode()), dati)
        for riga in ('DONE', 'ERR', 'CONFERMA 25 2', base64.b64encode(b'x' * 80).decode()):
            self.assertIsNone(estrai_salvataggio(riga))


class TestDatiCombattimento(unittest.TestCase):
    """Dati di battaglia Gen 1 (tools/dex_types.py, SPECIALE in dex_stats.py, dex_moves.py)."""

    @classmethod
    def setUpClass(cls):
        from dex_data import TYPE_ACCENTS
        from dex_stats import BASE_STATS, SPECIALE
        from dex_types import TIPI, CATEGORIA, TIPI_GEN1, TABELLA_TIPI
        cls.ACCENTS, cls.STATS, cls.SPECIALE = TYPE_ACCENTS, BASE_STATS, SPECIALE
        cls.TIPI, cls.CATEGORIA, cls.GEN1, cls.TABELLA = TIPI, CATEGORIA, TIPI_GEN1, TABELLA_TIPI

    def efficacia(self, att, dif):
        return self.TABELLA[self.TIPI.index(att)][self.TIPI.index(dif)]

    def test_i_tipi_sono_i_15_della_gen_1_con_un_colore(self):
        self.assertEqual(len(self.TIPI), 15)
        self.assertEqual(len(set(self.TIPI)), 15)
        self.assertEqual(set(self.TIPI), set(self.ACCENTS))

    def test_le_151_specie_hanno_tipi_validi(self):
        self.assertEqual(sorted(self.GEN1), list(range(1, 152)))
        for num, (t1, t2) in self.GEN1.items():
            self.assertIn(t1, self.TIPI, f'#{num}')
            self.assertIn(t2, self.TIPI + (None,), f'#{num}')
            self.assertNotEqual(t1, t2, f'#{num}: tipo doppio ripetuto')

    def test_la_speciale_e_plausibile_per_le_151(self):
        self.assertEqual(sorted(self.SPECIALE), list(range(1, 152)))
        for num, spc in self.SPECIALE.items():
            self.assertTrue(15 <= spc <= 160, f'#{num}: Speciale {spc}')
        self.assertEqual(self.SPECIALE[150], 154)   # Mewtwo
        self.assertEqual(self.SPECIALE[113], 105)   # Chansey
        self.assertEqual(self.SPECIALE[6], 85)      # Charizard

    def test_le_altre_statistiche_restano_quelle_attuali(self):
        self.assertEqual(self.STATS[1], (45, 49, 49, 45))
        self.assertTrue(all(len(v) == 4 for v in self.STATS.values()))

    def test_la_tabella_e_15x15_con_valori_ammessi(self):
        self.assertEqual(len(self.TABELLA), 15)
        for riga in self.TABELLA:
            self.assertEqual(len(riga), 15)
            for v in riga:
                self.assertIn(v, (0, 0.5, 1, 2))

    def test_categoria_per_tipo_come_in_gen_1(self):
        speciali = {t for t, c in self.CATEGORIA.items() if c == 'speciale'}
        self.assertEqual(speciali, {'fuoco', 'acqua', 'erba', 'elettro', 'ghiaccio',
                                    'psico', 'drago'})
        self.assertEqual(set(self.CATEGORIA), set(self.TIPI))
        self.assertTrue(set(self.CATEGORIA.values()) <= {'fisica', 'speciale'})

    def test_controlli_a_campione_di_rosso_blu(self):
        self.assertEqual(self.GEN1[35], ('normale', None))       # Clefairy
        self.assertEqual(self.GEN1[81], ('elettro', None))       # Magnemite
        self.assertEqual(self.GEN1[6], ('fuoco', 'volante'))     # Charizard
        self.assertEqual(self.efficacia('veleno', 'coleottero'), 2)
        self.assertEqual(self.efficacia('coleottero', 'veleno'), 2)
        self.assertEqual(self.efficacia('ghiaccio', 'fuoco'), 1)
        self.assertEqual(self.efficacia('spettro', 'psico'), 0)  # bug storico, per fedeltà
        self.assertEqual(self.efficacia('normale', 'spettro'), 0)
        self.assertEqual(self.efficacia('terra', 'volante'), 0)
        self.assertEqual(self.efficacia('acqua', 'fuoco'), 2)

    # --- Mosse e learnset (tools/dex_moves.py) ---

    def test_ogni_mossa_ha_valori_validi(self):
        from dex_moves import MOSSE, EFFETTI, STATI, STATISTICHE
        effetti = [n for n, _ in EFFETTI]
        self.assertEqual(len(effetti), len(set(effetti)))
        numeri = [v[0] for v in MOSSE.values()]
        self.assertEqual(len(numeri), len(set(numeri)), 'id Gen 1 ripetuti')
        for slug, (num, tipo, pot, prec, pp, eff, p1, p2) in MOSSE.items():
            self.assertTrue(1 <= num <= 165, slug)
            self.assertIn(tipo, self.TIPI, slug)
            self.assertTrue(0 <= pot <= 255 and 0 < prec <= 100, slug)
            self.assertTrue(0 < pp <= 40, f'{slug}: PP {pp}')
            self.assertIn(eff, effetti, slug)
            if eff in ('STADIO', 'STADIO_SECONDARIO'):
                self.assertTrue(0 <= p1 < len(STATISTICHE), slug)
            if eff in ('STATO', 'STATO_SECONDARIO'):
                self.assertTrue(0 < p1 < len(STATI), slug)
            if eff.endswith('SECONDARIO'):
                self.assertTrue(0 < p2 <= 100, f'{slug}: probabilità {p2}')

    def test_mosse_di_stato_esclusioni_e_trasformazione_come_nel_piano(self):
        from dex_moves import MOSSE, ESCLUSE, RIPIEGO
        for slug in ('thunder-wave', 'stun-spore', 'sleep-powder', 'hypnosis', 'sing',
                     'poison-powder', 'toxic', 'confuse-ray', 'supersonic'):
            self.assertEqual(MOSSE[slug][5], 'STATO', slug)
        self.assertEqual(MOSSE['rest'][5], 'RIPOSO')
        self.assertEqual(MOSSE['leech-seed'][5], 'PARASSISEME')
        self.assertEqual(MOSSE['transform'][5], 'TRASFORMAZIONE')
        for slug in ('metronome', 'mimic', 'mirror-move', 'counter', 'bide', 'substitute',
                     'disable'):
            self.assertIn(slug, ESCLUSE)
        self.assertFalse(set(ESCLUSE) & set(MOSSE))
        self.assertEqual(MOSSE[RIPIEGO][1:6], ('normale', 50, 100, 10, 'RINCULO'))

    def test_valori_di_rosso_blu_a_campione(self):
        from dex_moves import MOSSE
        self.assertEqual(MOSSE['karate-chop'][1:6], ('normale', 50, 100, 25, 'BRUTTO_COLPO'))
        self.assertEqual(MOSSE['bite'][1:3], ('normale', 60))
        self.assertEqual(MOSSE['gust'][1], 'normale')
        self.assertEqual(MOSSE['tackle'][2:4], (35, 95))
        self.assertEqual(MOSSE['psychic'][5:], ('STADIO_SECONDARIO', 3, 33))  # Speciale -1
        self.assertEqual(MOSSE['dragon-rage'][5:7], ('DANNO_FISSO', 40))

    def test_ogni_mossa_dei_learnset_esiste_e_ha_pp(self):
        from dex_moves import MOSSE, APPRENDIMENTO, RIPIEGO
        self.assertEqual(sorted(APPRENDIMENTO), list(range(1, 152)))
        for num, voci in APPRENDIMENTO.items():
            self.assertEqual(voci, sorted(voci, key=lambda v: v[0]), f'#{num}: livelli in disordine')
            for lv, slug in voci:
                self.assertIn(slug, MOSSE, f'#{num}: {slug}')
                self.assertNotEqual(slug, RIPIEGO, f'#{num}: Scontro non si impara')
                self.assertGreater(MOSSE[slug][4], 0, f'#{num}: {slug} senza PP')
                self.assertTrue(1 <= lv <= 100, f'#{num}: livello {lv}')

    def test_ogni_specie_a_ogni_livello_ha_una_mossa_o_scontro(self):
        from dex_moves import MOSSE, APPRENDIMENTO, RIPIEGO
        self.assertGreater(MOSSE[RIPIEGO][4], 0)
        senza = {num for num, voci in APPRENDIMENTO.items()
                 for lv in range(1, 101) if not any(l <= lv for l, _ in voci)}
        self.assertEqual(senza, {63})  # Abra: solo Teletrasporto, escluso -> Scontro
        self.assertEqual(APPRENDIMENTO[129], [(1, 'splash'), (15, 'tackle')])  # Magikarp
        self.assertEqual(APPRENDIMENTO[132], [(1, 'transform')])               # Ditto

    # --- Nomi delle mosse (tools/dex_move_names.py) ---

    def test_ogni_mossa_ha_il_nome_in_tutte_le_lingue(self):
        from dex_moves import MOSSE
        from dex_move_names import MOVE_NAMES
        self.assertEqual(set(MOVE_NAMES), set(MOSSE))
        for slug, nomi in MOVE_NAMES.items():
            # il PT non c'è: ricade sull'inglese, come per le specie
            self.assertEqual(set(nomi), {'en', 'es', 'fr', 'de', 'it', 'ja', 'ko'}, slug)

    def test_i_nomi_delle_mosse_rispettano_i_limiti_in_tutte_le_lingue(self):
        from dex_move_names import MOVE_NAMES, MAX_MOSSA, MAX_MOSSA_CJK
        self.assertEqual((MAX_MOSSA, MAX_MOSSA_CJK), (12, 7))
        for lang in ('ES', 'EN', 'FR', 'DE', 'IT', 'PT', 'JA', 'KO'):
            lg = lang.lower() if lang != 'PT' else 'en'
            visti = {}
            for slug, nomi in MOVE_NAMES.items():
                nm = nomi[lg]
                self.assertTrue(nm and nm == nm.strip(), f'{slug} {lang}: {nm!r}')
                if lg in ('ja', 'ko'):
                    # caratteri, non byte: in UTF-8 ogni kana o sillaba occupa 3
                    self.assertLessEqual(len(nm), MAX_MOSSA_CJK, f'{slug} {lang}: {nm}')
                    self.assertNotIn('"', nm)
                else:
                    self.assertRegex(nm, r"^[A-Z0-9 .'\-]+$", f'{slug} {lang}: font CP437, maiuscolo ASCII')
                    self.assertLessEqual(len(nm), MAX_MOSSA, f'{slug} {lang}: {nm}')
                self.assertNotIn(nm, visti, f'{lang}: {slug} e {visti.get(nm)} hanno lo stesso nome')
                visti[nm] = slug

    def test_i_nomi_cjk_stanno_nei_font(self):
        from dex_move_names import MOVE_NAMES
        for slug, nomi in MOVE_NAMES.items():
            # korean2 ha le sillabe di KS X 1001, cioè quanto codifica euc_kr
            nomi['ko'].encode('euc_kr')
            for c in nomi['ja']:
                self.assertTrue(c.isascii() or '\u3041' <= c <= '\u30fc',
                                f'{slug}: {c!r} non è kana né ASCII')

    def test_le_evoluzioni_per_scambio_come_nel_piano(self):
        from dex_data import DEX, EVO_SCAMBIO
        self.assertEqual(EVO_SCAMBIO, {64: 65, 67: 68, 75: 76, 93: 94})
        evolve = {num: evo for num, _s, _d, _t, evo, _l in DEX}
        for num, evo in EVO_SCAMBIO.items():
            self.assertEqual(evolve[num], evo, f'#{num}: resta anche il percorso a livello')


class TestMusica(unittest.TestCase):
    """Le musiche di battaglia e dei minigiochi (tools/gen_musica.py), brani originali in
    tools/musica/originali/: solo comandi che l'interprete conosce, salti su un comando del
    brano, brani in ciclo, vittorie con una fine, canali che si riallineano, batteria del
    canale 4."""

    RADICE = os.path.join(TOOLS, 'musica')
    BATTAGLIE = ('MUS_WILDBATTLE', 'MUS_TRAINERBATTLE', 'MUS_GYMLEADERBATTLE', 'MUS_FINALBATTLE')
    VITTORIE = ('MUS_DEFEATEDWILDMON', 'MUS_DEFEATEDTRAINER', 'MUS_DEFEATEDGYMLEADER')
    MINIGIOCHI = ('MUS_SURFINGPIKACHU', 'MUS_DUNGEON2')

    @classmethod
    def setUpClass(cls):
        import gen_musica
        cls.gm = gen_musica
        cls.dati = gen_musica.assembla()

    def comandi(self, brano):
        return list(self.gm.istruzioni_brano(brano))

    def test_i_nove_brani_e_i_loro_canali(self):
        self.assertEqual([b['enum'] for b in self.dati['brani']],
                         list(self.BATTAGLIE + self.VITTORIE + self.MINIGIOCHI))
        for b in self.dati['brani']:
            # solo Dungeon2 ha il quarto canale, la batteria
            self.assertEqual(len(b['inizio']), 4 if b['enum'] == 'MUS_DUNGEON2' else 3, b['file'])
            self.assertEqual(b['ripeti'], b['enum'] not in self.VITTORIE, b['file'])
        self.assertEqual({b['sorgente'] for b in self.dati['brani']}, {'originali'})
        # nel repository ci sono solo i brani originali, niente sorgenti di pret
        self.assertEqual(sorted(n for n in os.listdir(self.RADICE) if not n.startswith('.')),
                         ['originali'])

    def test_solo_comandi_conosciuti(self):
        noti = {self.gm.C_TOGGLE_PERFECT_PITCH, self.gm.C_VIBRATO, self.gm.C_DUTY_CYCLE,
                self.gm.C_TEMPO, self.gm.C_VOLUME, self.gm.C_EXECUTE_MUSIC,
                self.gm.C_SOUND_CALL, self.gm.C_SOUND_LOOP, self.gm.C_SOUND_RET}
        for b in self.dati['brani']:
            cmd = self.comandi(b)
            ultimo, primo, rumore = cmd[-1]
            # il bytecode si percorre fino all'ultimo byte senza avanzi
            self.assertEqual(ultimo + self.gm.lunghezza_comando(primo, rumore), len(b['dati']),
                             b['file'])
            for off, x, rumore in cmd:
                self.assertTrue(x < 0xE8 or x in noti, f'{b["file"]}: ${x:02X} a ${off:04X}')
                if rumore:  # sul canale 4 niente note: drum_note, pause e drum_speed
                    self.assertTrue(x >= 0xB0, f'{b["file"]}: ${x:02X} a ${off:04X}')
                    if x & 0xF0 == self.gm.C_DRUM_NOTE:
                        self.assertTrue(1 <= b['dati'][off + 1] <= len(self.dati['rumori']))

    def test_gli_strumenti_di_rumore(self):
        rumori = self.dati['rumori']
        self.assertEqual([n for n, _ in self.gm.RUMORI],
                         ['cassa', 'rullante', 'charleston chiuso', 'charleston aperto', 'piatto'])
        self.assertEqual(len(rumori), len(self.gm.RUMORI))
        for i, note in enumerate(rumori, 1):
            self.assertTrue(1 <= len(note) <= 6, i)
            for lung, nr42, nr43 in note:
                self.assertTrue(0 <= lung <= 15 and nr42 >> 4 > 0 and 0 <= nr43 <= 255, i)
        # la cassa scende: tre note con lo spostamento di NR43 che cresce, volume 13 e fade 1
        self.assertEqual(rumori[0][0], (0, 0xD1, 0x55))
        self.assertEqual([n[2] >> 4 for n in rumori[0]], [5, 6, 7])
        # il piatto suona più a lungo del charleston chiuso
        self.assertGreater(rumori[4][0][0], rumori[2][0][0])

    def test_note_e_forme_d_onda_calcolate(self):
        # La a 110 Hz nell'ottava 1: 2048 - 131072 / 110 = 856,4 -> -1191,6 -> $FB58
        self.assertEqual(self.dati['note'][9], 0xFB58)
        self.assertEqual(self.dati['note'][0], 0xF82C)  # Do2, 65,4 Hz
        quadra, triangolo, sega, organo = self.dati['onde']
        self.assertEqual(quadra, [15] * 16 + [0] * 16)
        self.assertEqual(triangolo[0], 0)
        self.assertEqual(max(triangolo), 15)
        self.assertEqual(sega, sorted(sega))
        self.assertEqual((min(organo), max(organo)), (0, 15))

    def test_i_salti_cadono_su_un_comando_del_brano(self):
        for b in self.dati['brani']:
            cmd = self.comandi(b)
            offset = {off for off, _, _ in cmd}
            canali = sorted(b['inizio'])
            for off, x, _ in cmd:
                if x not in (self.gm.C_SOUND_CALL, self.gm.C_SOUND_LOOP):
                    continue
                dest = self.gm.bersaglio(b['dati'], off)
                self.assertIn(dest, offset, f'{b["file"]}: salto a ${dest:04X}')
                if x == self.gm.C_SOUND_LOOP:
                    # un ciclo torna indietro dentro il suo canale
                    inizio = max(c for c in canali if c <= off)
                    self.assertTrue(inizio <= dest <= off, f'{b["file"]}: ciclo a ${off:04X}')

    def test_battaglie_in_ciclo_e_vittorie_con_una_fine(self):
        for b in self.dati['brani']:
            infiniti = [off for off, x, _ in self.comandi(b)
                        if x == self.gm.C_SOUND_LOOP and b['dati'][off + 1] == 0]
            for c, r in enumerate(self.gm.simula(b)):
                with self.subTest(brano=b['file'], canale=c + 1):
                    if b['ripeti']:
                        self.assertIsNone(r['fine'])
                        self.assertGreaterEqual(len(r['cicli']), 11)
                    else:
                        self.assertIsNotNone(r['fine'])
                        self.assertEqual(r['cicli'], [])
            self.assertEqual(bool(infiniti), b['ripeti'], b['file'])

    def test_i_canali_restano_allineati_per_dieci_giri(self):
        for enum in self.BATTAGLIE + self.VITTORIE + self.MINIGIOCHI:
            righe = [r for r in self.gm.durate(self.dati) if r[0] == enum]
            for _e, canale, tipo, _primo, giri in righe:
                if tipo == 'ciclo':
                    self.assertEqual(len(set(giri)), 1, f'{enum} canale {canale}: {giri}')
            if righe[0][2] == 'fine':
                self.assertEqual(len({r[3] for r in righe}), 1, f'{enum}: canali sfasati {righe}')
                continue
            # stessa introduzione, e ogni giro divide il più lungo: i canali si riallineano
            # (in Dungeon2 il canale 3 gira ogni 6048 fotogrammi, l'1 e il 2 ogni 3024)
            self.assertEqual(len({r[3] - r[4][0] for r in righe}), 1, f'{enum}: {righe}')
            lungo = max(r[4][0] for r in righe)
            for r in righe:
                self.assertEqual(lungo % r[4][0], 0, f'{enum} canale {r[1]}: {r[4][0]}')

    def test_durate_plausibili(self):
        fps = self.gm.FPS_NUM / self.gm.FPS_DEN
        brani = {}
        for enum, _c, tipo, primo, giri in self.gm.durate(self.dati):
            secondi = (giri[0] if tipo == 'ciclo' else primo) / fps
            brani[enum] = max(brani.get(enum, 0), secondi)
        for enum, secondi in brani.items():
            self.assertTrue(5 < secondi < 120, f'{enum}: {secondi:.1f} s')

    def test_dimensioni(self):
        self.assertEqual(len(self.dati['note']), 12)
        self.assertEqual(len(self.dati['onde']), 4)
        totale = sum(len(b['dati']) for b in self.dati['brani'])
        self.assertLess(totale, 10 * 1024, 'il bytecode deve restare piccolo in flash')
        with open(os.path.join(ROOT, 'musica_dati.h'), encoding='utf-8') as fh:
            testo = fh.read()
        self.assertTrue(testo.isascii(), 'musica_dati.h deve essere ASCII')
        for b in self.dati['brani']:
            self.assertIn(f'MUSICA_DATI_{b["enum"][4:]}[{len(b["dati"])}]', testo)

    def sorgenti_modificati(self, file, cambia):
        """Una copia di tools/musica/ con `file` di originali/ passato per `cambia`."""
        tmp = tempfile.mkdtemp(prefix='pokegotchi-musica-')
        self.addCleanup(shutil.rmtree, tmp, ignore_errors=True)
        cartella = os.path.join(tmp, 'musica')
        shutil.copytree(self.RADICE, cartella, ignore=shutil.ignore_patterns('.cache_pret'))
        path = os.path.join(cartella, 'originali', 'audio', 'music', file)
        with open(path, encoding='utf-8') as fh:
            testo = fh.read()
        nuovo = cambia(testo)
        self.assertNotEqual(nuovo, testo, 'la modifica di prova non ha cambiato nulla')
        with open(path, 'w', encoding='utf-8') as fh:
            fh.write(nuovo)
        return cartella

    def assertErrore(self, cartella, frammento):
        with self.assertRaises(self.gm.ErroreMusica) as e:
            self.gm.assembla(cartella)
        self.assertIn(frammento, str(e.exception))

    def test_errore_su_un_comando_sconosciuto(self):
        cartella = self.sorgenti_modificati(
            'lotta_selvatico.asm', lambda t: t.replace('\ttempo 104\n', '\ttempo 104\n\tpitch_slide 1, 4, C_\n', 1))
        self.assertErrore(cartella, 'comando sconosciuto')
        # da riga di comando: uscita diversa da 0 e nessun header scritto
        tmp = tempfile.mkdtemp(prefix='pokegotchi-musica-')
        self.addCleanup(shutil.rmtree, tmp, ignore_errors=True)
        shutil.copytree(TOOLS, os.path.join(tmp, 'tools'),
                        ignore=shutil.ignore_patterns('musica', '__pycache__'))
        shutil.copytree(cartella, os.path.join(tmp, 'tools', 'musica'))
        r = subprocess.run([sys.executable, os.path.join(tmp, 'tools', 'gen_musica.py')],
                           capture_output=True, text=True)
        self.assertNotEqual(r.returncode, 0)
        self.assertIn('comando sconosciuto', r.stderr)
        self.assertFalse(os.path.exists(os.path.join(tmp, 'musica_dati.h')))

    def test_errore_su_un_etichetta_mancante(self):
        cartella = self.sorgenti_modificati(
            'lotta_finale.asm', lambda t: t.replace('\n.a:', '\n.z:', 1))
        self.assertErrore(cartella, 'etichetta mancante')

    def test_errore_su_un_canale_senza_fine_ne_ciclo(self):
        cartella = self.sorgenti_modificati(
            'grotta.asm', lambda t: t.replace('\tsound_loop 0, .mainloop\n', '', 1))
        self.assertErrore(cartella, 'né fine né ciclo')
        cartella = self.sorgenti_modificati(
            'vittoria_selvatico.asm', lambda t: t[:t.rindex('\tsound_ret')])
        self.assertErrore(cartella, 'né fine né ciclo')

    def test_errore_su_un_ciclo_in_avanti(self):
        cartella = self.sorgenti_modificati(
            'lotta_selvatico.asm', lambda t: t.replace(
                '\tsound_loop 0, .mainloop\n',
                '\tsound_loop 2, .avanti\n.avanti:\n\tsound_loop 0, .mainloop\n', 1))
        self.assertErrore(cartella, 'non torna indietro')

    def test_errore_sui_comandi_della_batteria_fuori_posto(self):
        cartella = self.sorgenti_modificati(
            'lotta_selvatico.asm', lambda t: t.replace('\ttempo 104\n', '\ttempo 104\n\tdrum_note 1, 4\n', 1))
        self.assertErrore(cartella, 'fuori dal canale 4')
        cartella = self.sorgenti_modificati(
            'grotta.asm', lambda t: t.replace('\tdrum_speed 12\n', '\tdrum_speed 12\n\tnote C_, 4\n', 1))
        self.assertErrore(cartella, 'nel canale 4')
        cartella = self.sorgenti_modificati(
            'grotta.asm', lambda t: t.replace('\tdrum_note 1, 4\n', '\tdrum_note 6, 4\n', 1))
        self.assertErrore(cartella, 'strumento 6')


class TestPmdJs(unittest.TestCase):
    """web/pmd.js (installer e PWA) converte gli sprite di SpriteCollab byte per byte come
    tools/pack_pmd.py e tools/make_thumbs.py. Gli sprite di prova sono disegnati qui, con
    un codificatore PNG proprio che usa tutti e cinque i filtri: RGBA, palette con tRNS (a 8
    e a 4 bit), più di 255 colori, alfa intorno alla soglia, alias CopyOf, un PNG che manca,
    fotogrammi fuori dall'immagine e oltre il massimo di 24."""

    # dimensioni dell'Idle per specie; la 4 scende a 20,42 x 40, da arrotondare
    VARIANTI = {1: (24, 32), 2: (56, 64), 3: (80, 40), 4: (49, 96)}

    @classmethod
    def setUpClass(cls):
        if not shutil.which('node'):
            raise unittest.SkipTest('node non installato')
        cls.tmp = tempfile.mkdtemp(prefix='pokegotchi-pmd-')
        cls.cache = os.path.join(cls.tmp, 'cache')
        cls.out = os.path.join(cls.tmp, 'mons')
        os.makedirs(cls.out)
        for dex, (fw, fh) in cls.VARIANTI.items():
            cls.disegna_specie(os.path.join(cls.cache, f'{dex:04d}'), dex, fw, fh)

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    # ------------------------------------------------------------ PNG e sprite di prova

    @staticmethod
    def png(w, h, tipo, righe, prof=8, plte=None, trns=None):
        """Un PNG con le righe grezze `righe`; il filtro della riga y è y % 5."""
        canali = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[tipo]
        bpp = max(1, canali * prof // 8)
        grezzi, prec = bytearray(), bytes(len(righe[0]))
        for y, r in enumerate(righe):
            f = y % 5
            grezzi.append(f)
            for x in range(len(r)):
                a = r[x - bpp] if x >= bpp else 0
                b = prec[x]
                c = prec[x - bpp] if x >= bpp else 0
                if f == 0:
                    v = r[x]
                elif f == 1:
                    v = r[x] - a
                elif f == 2:
                    v = r[x] - b
                elif f == 3:
                    v = r[x] - ((a + b) >> 1)
                else:
                    q = a + b - c
                    pa, pb, pc = abs(q - a), abs(q - b), abs(q - c)
                    v = r[x] - (a if pa <= pb and pa <= pc else b if pb <= pc else c)
                grezzi.append(v & 0xFF)
            prec = r

        def blocco(t, d):
            return (len(d).to_bytes(4, 'big') + t + d
                    + zlib.crc32(t + d).to_bytes(4, 'big'))
        z = zlib.compress(bytes(grezzi))
        out = b'\x89PNG\r\n\x1a\n' + blocco(b'IHDR', w.to_bytes(4, 'big') + h.to_bytes(4, 'big')
                                             + bytes([prof, tipo, 0, 0, 0]))
        if plte:
            out += blocco(b'PLTE', bytes(c for rgb in plte for c in rgb))
        if trns:
            out += blocco(b'tRNS', bytes(trns))
        metà = len(z) // 2  # due IDAT
        return out + blocco(b'IDAT', z[:metà]) + blocco(b'IDAT', z[metà:]) + blocco(b'IEND', b'')

    @classmethod
    def rgba(cls, w, h, pixel):
        return cls.png(w, h, 6, [bytes(c for x in range(w) for c in pixel(x, y)) for y in range(h)])

    @classmethod
    def disegna_specie(cls, cartella, v, fw, fh):
        os.makedirs(cartella)
        # 12 colori; (8, 8, 8) e (9, 9, 9) coincidono in RGB565
        colori = [(8, 8, 8), (9, 9, 9), (200, 30, 40), (30, 200, 40), (40, 30, 200), (255, 255, 255),
                  (0, 0, 0), (128, 64, 32), (250, 250, 5), (5, 250, 250), (100, 100, 100), (17, 34, 51)]

        def idle(x, y):
            a = (x * 7 + y * 3 + v) % 11
            return colori[(x // 3 + y // 4 + v) % 12] + ((0, 127, 128)[a] if a < 3 else 255,)
        file = {'Idle-Anim.png': cls.rgba(fw * 3, fh * 8, idle)}
        # Walk: palette con trasparenza (indice 0 trasparente, 1 a 127, 2 a 128); la specie 2 a 4 bit
        plte = [(10, 20, 30), (40, 50, 60), (70, 80, 90), (200, 0, 0), (0, 200, 0), (0, 0, 200)]
        prof = 4 if v == 2 else 8
        righe = []
        for y in range(24 * 8):
            idx = [(x // 2 + y // 3 + v) % 6 for x in range(16 * 4)]
            righe.append(bytes(idx) if prof == 8 else
                         bytes(idx[i] << 4 | idx[i + 1] for i in range(0, len(idx), 2)))
        file['Walk-Anim.png'] = cls.png(64, 192, 3, righe, prof, plte, [0, 127, 128])
        # Attack: un colore per pixel, oltre i 255 della palette
        file['Attack-Anim.png'] = cls.rgba(40, 20, lambda x, y: ((x * 37 + v) % 256, (y * 91 + x) % 256,
                                                                 (x * y * 13) % 256, 255))
        file['Hurt-Anim.png'] = cls.rgba(32, 20, lambda x, y: (1, 2, 3, 255))       # nessun fotogramma
        file['Pose-Anim.png'] = cls.rgba(32, 30, lambda x, y: (60, 70, 80, 255))    # più bassa del fotogramma
        file['Nod-Anim.png'] = cls.rgba(4 * 30, 4, lambda x, y: (x % 7 * 30, 9, 9, 255))  # 30 fotogrammi
        for nome, dati in file.items():
            with open(os.path.join(cartella, nome), 'wb') as fh_:
                fh_.write(dati)

        def anim(nome, w, h, durate):
            d = ''.join(f'<Duration>{x}</Duration>' for x in durate)
            return (f'<Anim><Name>{nome}</Name><Index>0</Index><FrameWidth>{w}</FrameWidth>'
                    f'<FrameHeight>{h}</FrameHeight><Durations>{d}</Durations></Anim>')
        xml = ('<?xml version="1.0" ?>\n<AnimData><ShadowSize>1</ShadowSize><Anims>\n'
               + anim('Walk', 16, 24, [4, 4, 5, 4]) + '\n'
               + anim('Idle', fw, fh, [4, 9, 30, 7]) + '\n'
               + '<Anim><Name>Sleep</Name><CopyOf>Idle</CopyOf></Anim>\n'
               + '<Anim><Name>Hop</Name><CopyOf>Inesistente</CopyOf></Anim>\n'
               + '<!-- un commento -->\n'
               + anim('Eat', 16, 16, [3, 3]) + '\n'        # il PNG manca
               + anim('Attack', 20, 20, [2, 6]) + '\n'
               + anim('Hurt', 64, 20, [3, 3]) + '\n'
               + anim('Pose', 16, 40, [5, 5]) + '\n'
               + anim('Nod', 4, 4, list(range(1, 31))) + '\n'
               + '</Anims></AnimData>\n')
        with open(os.path.join(cartella, 'AnimData.xml'), 'w', encoding='utf-8') as fh_:
            fh_.write(xml)

    # ------------------------------------------------------------ confronti

    def pack_python(self, dex, shiny=False):
        sys.path.insert(0, TOOLS)
        import pack_pmd
        vecchi = pack_pmd.CACHE, pack_pmd.OUT, pack_pmd.fetch
        pack_pmd.CACHE, pack_pmd.OUT = self.cache, self.out
        pack_pmd.fetch = lambda url, dest: os.path.exists(dest)  # niente rete
        try:
            pack_pmd.pack(dex, shiny)
        finally:
            pack_pmd.CACHE, pack_pmd.OUT, pack_pmd.fetch = vecchi
        with open(os.path.join(self.out, f'p{dex:03d}.bin'), 'rb') as f:
            return f.read()

    def node(self, *arg):
        r = subprocess.run(['node', os.path.join(ROOT, 'test', 'pmd_node.mjs'), *arg],
                           capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stderr)

    def test_gli_sprite_sono_identici_a_quelli_di_pack_pmd(self):
        for dex in self.VARIANTI:
            with self.subTest(dex=dex):
                atteso = self.pack_python(dex)
                uscita = os.path.join(self.tmp, f'js{dex}.bin')
                self.node('specie', os.path.join(self.cache, f'{dex:04d}'), uscita)
                with open(uscita, 'rb') as f:
                    self.assertEqual(f.read(), atteso)
                # le azioni attese: Idle, Walk sx/dx, Sleep (alias), Attack, Hurt (0 fotogrammi),
                # Pose e Nod; niente Eat (PNG mancante) né Hop (alias di un'animazione che manca)
                self.assertEqual(atteso[:4], b'PGK2')
                self.assertEqual(atteso[4], 8)
                self.assertEqual(int.from_bytes(atteso[5:7], 'little'), 255)  # palette piena

    def test_le_miniature_sono_identiche_a_quelle_di_make_thumbs(self):
        sys.path.insert(0, TOOLS)
        import make_thumbs
        cartella = os.path.join(self.tmp, 'miniature')
        os.makedirs(cartella, exist_ok=True)
        sprite = {dex: self.pack_python(dex) for dex in self.VARIANTI}
        elenco = []
        for i in range(1, 152):
            dex = (i - 1) % len(self.VARIANTI) + 1
            with open(os.path.join(cartella, f'p{i:03d}.bin'), 'wb') as f:
                f.write(sprite[dex])
            elenco.append(os.path.join(self.cache, f'{dex:04d}'))
        vecchia = make_thumbs.DIR
        make_thumbs.DIR = cartella
        try:
            make_thumbs.main()
        finally:
            make_thumbs.DIR = vecchia
        with open(os.path.join(cartella, 'thumbs.bin'), 'rb') as f:
            atteso = f.read()
        # la PWA le fa dalla sola Idle: stesse miniature
        json_elenco = os.path.join(self.tmp, 'elenco.json')
        with open(json_elenco, 'w', encoding='utf-8') as f:
            f.write(repr(elenco).replace("'", '"'))
        uscita = os.path.join(self.tmp, 'thumbs-js.bin')
        self.node('miniature', json_elenco, uscita)
        with open(uscita, 'rb') as f:
            self.assertEqual(f.read(), atteso)
        self.assertEqual(atteso[:4], b'PGTH')
        # la specie 2 (56x64) scende a 35x40, la 3 (80x40) a 40x20, la 4 (49x96) a 20x40
        off = [int.from_bytes(atteso[6 + 4 * i:10 + 4 * i], 'little') for i in range(4)]
        self.assertEqual(tuple(atteso[off[0]:off[0] + 2]), (24, 32))
        self.assertEqual(tuple(atteso[off[1]:off[1] + 2]), (35, 40))
        self.assertEqual(tuple(atteso[off[2]:off[2] + 2]), (40, 20))
        self.assertEqual(tuple(atteso[off[3]:off[3] + 2]), (20, 40))

    def test_stesso_commit_di_spritecollab_nei_due_convertitori(self):
        sys.path.insert(0, TOOLS)
        import pack_pmd
        js = leggi_sorgente('web/pmd.js')
        self.assertRegex(pack_pmd.COMMIT, r'^[0-9a-f]{40}$')
        self.assertIn(f"export const COMMIT = '{pack_pmd.COMMIT}';", js)
        self.assertIn(pack_pmd.COMMIT, pack_pmd.BASE)


class TestAvvisi(unittest.TestCase):
    """Licenze di terzi e autori degli sprite: completi, e copiati dove si distribuisce."""

    def test_le_licenze_di_terzi_ci_sono_tutte(self):
        t = leggi_sorgente('THIRD_PARTY_NOTICES.md')
        for nome in ('GFX Library for Arduino', 'SensorLib', 'XPowersLib', 'U8g2', 'GNU Unifont',
                     'GNU LESSER GENERAL PUBLIC LICENSE', 'Apache License', 'Emscripten', 'musl',
                     'SDL2', 'GNU GENERAL PUBLIC LICENSE', 'ESP Web Tools', 'mbedTLS'):
            self.assertIn(nome, t)
        self.assertEqual(t.count('```') % 2, 0)

    def test_i_crediti_degli_sprite_hanno_le_151_specie_e_nessun_id_discord(self):
        t = leggi_sorgente('docs/CREDITI_SPRITE.md')
        righe = [r for r in t.splitlines() if re.match(r'^\| \d+ \|', r)]
        self.assertEqual(len(righe), 151)
        self.assertNotIn('<@', t)
        sys.path.insert(0, TOOLS)
        import pack_pmd
        self.assertIn(pack_pmd.COMMIT, t)

    def test_avvisi_e_crediti_vanno_nelle_release_nel_sito_e_nella_pwa(self):
        self.assertIn('THIRD_PARTY_NOTICES.md', leggi_sorgente('.github/workflows/release.yml'))
        for script in ('tools/build_sito.sh', 'tools/build_pwa.sh'):
            t = leggi_sorgente(script)
            self.assertIn('THIRD_PARTY_NOTICES', t, script)
            self.assertIn('CREDITI_SPRITE', t, script)
        self.assertIn('CREDITI_SPRITE.txt', leggi_sorgente('pwa/sw.js'))


class TestVersione(unittest.TestCase):
    """Una sola versione, in semver, ovunque compaia; release-please la aggiorna in tutti i
    file (extra-files) e la riconosce nell'header (il commento x-release-please-version)."""

    def test_la_versione_coincide_ovunque(self):
        import json
        riga = next(r for r in leggi_sorgente('ui_common.h').splitlines()
                    if r.startswith('#define FW_VERSION'))
        self.assertIn('x-release-please-version', riga)
        fw = re.search(r'"([^"]*)"', riga).group(1)
        self.assertRegex(fw, r'^\d+\.\d+\.\d+$')
        self.assertEqual(json.loads(leggi_sorgente('web/manifest.json'))['version'], fw)
        self.assertEqual(leggi_sorgente('version.txt').strip(), fw)
        rilasciata = json.loads(leggi_sorgente('.release-please-manifest.json'))['.']
        chiave = lambda v: tuple(int(x) for x in v.split('.'))
        self.assertLessEqual(chiave(rilasciata), chiave(fw), 'il firmware è dietro all\'ultima release')

    def test_release_please_aggiorna_i_file_della_versione(self):
        import json
        conf = json.loads(leggi_sorgente('release-please-config.json'))
        extra = {e['path'] for e in conf['packages']['.']['extra-files']}
        self.assertEqual(extra, {'ui_common.h', 'web/manifest.json'})
        self.assertEqual(conf['release-type'], 'simple')  # version.txt e CHANGELOG.md


class TestNomeProgetto(unittest.TestCase):
    """Il nome del progetto è uno solo. Il nome che aveva fino alla 2.1 (quello del progetto
    originale) resta solo nei crediti verso i repository da cui deriva e nei tre file che
    migrano partite e salvataggi di allora. Le parole cercate sono spezzate, così questo
    file non le contiene."""

    VECCHIO = re.compile('tama' + r'\W?' + 'poke', re.I)
    # i repository da cui il progetto deriva (crediti e avvisi MIT): owner/nome o fork
    CREDITO = re.compile(r'(socquique|DylanPDao|ShadowEnemyx|allenst486db)/i?' + 'Tama' + 'Poke'
                         r'|i' + 'Tama' + r'Poke|' + 'tama' + r'poke-expanded|' + 'Tama' + r'Poke — Expanded'
                         r'|makerworld\.com/\S*')
    MIGRAZIONE = {'savefile.h', 'pwa/salvataggi.js', 'tools/salvataggi.py'}

    @staticmethod
    def _file_di_testo():
        nomi = subprocess.run(['git', 'ls-files'], cwd=ROOT, capture_output=True, text=True,
                              check=True).stdout.split('\n')
        for nome in filter(None, nomi):
            if nome.endswith(('.bin', '.png', '.gif', '.wasm', '.woff2', '.ttf', '.pdf', '.pgsave')):
                continue
            percorso = os.path.join(ROOT, nome)
            if not os.path.isfile(percorso):
                continue
            with open(percorso, 'rb') as f:
                yield nome, f.read().decode('utf-8', 'replace')

    def test_il_nome_vecchio_resta_solo_nei_crediti_e_nelle_migrazioni(self):
        trovati = []
        for nome, testo in self._file_di_testo():
            if nome in self.MIGRAZIONE:
                continue
            for n, riga in enumerate(testo.split('\n'), 1):
                if self.VECCHIO.search(self.CREDITO.sub('', riga)):
                    trovati.append(f'{nome}:{n}: {riga.strip()[:90]}')
        self.assertEqual(trovati, [])


if __name__ == '__main__':
    unittest.main(verbosity=2)
