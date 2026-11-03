# Changelog

Tutte le modifiche rilevanti al firmware. Le versioni coincidono con il numero mostrato in
fondo alla schermata dell'orologio/impostazioni (scorri verso il basso) e con `web/manifest.json`.

Aggiornare dall'installer web (la pagina «Installa» del sito) **senza** spuntare "Erase
device" mantiene il tuo Pokémon; dalla 3.0 la scheda si aggiorna anche da sola via WiFi.

Fino alla 2.1 il progetto usava il nome del progetto originale da cui deriva (vedi
`CREDITS.md`); i nomi nelle voci qui sotto sono stati allineati al nome attuale.

## [2.1] - 2026-10-31

### Aggiunto

- **Minigiochi a movimento** con l'accelerometro della scheda (QMI8658): il pulsante ⚽ apre la
  scelta tra la palla, la **Grotta Diglett** e il **Surf**. All'inizio uno zero di 1,5 s
  («tienila come per giocare»), così si gioca anche sdraiati; senza sensore il dito tenuto
  fa da joystick.
- **Grotta Diglett**: si inclina la scheda e il pet scivola nell'arena, raccoglie bacche e
  Caramelle Rare e schiva Diglett, massi di Frana e Dugtrio. Tre colpi o 90 s. Allena la
  **DIFESA** (punti / 4, al massimo +18 a partita), che finora cresceva solo con la buona cura.
- **Surf**, dalla Spiaggia di Pikachu di Giallo, per tutte le specie: spinta in discesa,
  salti dalla cresta, giri della tavola in aria, atterraggio entro ±25° dall'onda (±40° e
  tavola gialla per Pikachu, Raichu e i tipi Acqua). Tre cadute o 90 s; nessuna statistica.
- Record della Grotta e del Surf, del giocatore, nel salvataggio (`PetSave` v5; i `.pgsave`
  vecchi arrivano a 0).
- **Musica dei minigiochi**: Dungeon2 di Rosso/Blu nella Grotta, con la batteria del canale
  del rumore (gli strumenti di percussione di pokered), e Surfing Pikachu di Giallo nel Surf.
  Rispettano l'interruttore MUSICA.
- **Scossone e colpetti** sulla schermata principale: uno scossone spaventa il pet (−10
  felicità, poi 60 s di pausa; nel sonno −5 e continua a dormire; l'uovo oscilla), un doppio
  colpetto sulla scocca riaccende lo schermo attenuato o fa voltare il pet. Spenti con lo
  schermo spento dal PWR e dall'interruttore **MOVIMENTO** nel pannello Audio (chiave `mot`).
- PWA: il sensore del telefono (DeviceMotion; su iOS il permesso al primo tocco, in `https`),
  con lo scossone ma senza i colpetti. Emulatore: I/J/K/L inclinano, Z scuote, C dà il
  doppio colpetto; nel copione `INCLINA`, `GIRA`, `SCUOTI` e `COLPETTI`.
- Console: `MOTO [SPENTO|VEGLIA|GIOCO]` (sensore, gravità ed eventi per 5 s); a fine partita
  le righe `GROTTA …` e `SURF …`.
- `tools/prova_imu/`: lo sketch che ha misurato il sensore (assi, rumore, scossone,
  camminata, bus condiviso con il touch).

### Modificato

- Sulla scheda ogni accesso al bus I2C (touch, PMU, RTC, codec, sensore) passa da un mutex:
  il sensore si legge da un task sul core 0 e, senza, il touch corrompeva ~2 % dei campioni.
- README, `COMANDI.md` (scelta dei giochi, Grotta, Surf, scossone e colpetti, pillola
  MOVIMENTO, limiti della PWA) aggiornati; nuove foto in `docs/img/schermate/`.
- La versione 2.1 è uguale in firmware, installer (`web/manifest.json`), service worker
  della PWA e «PokeGotchi finto» di `tools/peer_link`.

## [2.0] - 2026-10-24

### Aggiunto

- **PokeGotchi nel browser e sul telefono (PWA)**: il link «Gioca nel browser» dell'installer
  web apre il firmware intero compilato in WebAssembly, con lo stesso gioco, le stesse
  regole e lo stesso motore audio della scheda. Si installa sulla schermata Home di iPhone e
  Android o come app in Chrome/Edge, gioca offline dopo «Scarica gli sprite per giocare
  offline» (gli sprite PMD arrivano da `sprites.pak` uno alla volta e restano nel browser)
  e si aggiorna con «Nuova versione pronta · Aggiorna» senza perdere la partita. Al primo
  avvio usa la lingua del browser.
- **Salvataggi nella PWA**: la partita resta nel browser (IndexedDB) e recupera il tempo
  passato da chiusa; il pannello col dischetto esporta e importa gli stessi `.pgsave` della
  scheda (foglio di condivisione sul telefono), tiene 3 copie automatiche, porta la partita
  dalla scheda e sulla scheda via Web Serial e ricorda di esportare dopo 7 giorni.
- **Emulatore desktop** (`tools/emu`, SDL2): il firmware intero sul PC, con suono, due
  istanze collegate via UDP e lo smoke test di 48 h; le foto delle schermate della
  documentazione vengono da qui (`tools/foto_doc.py`, in `docs/img/schermate/`).
- `COMANDI.md`: le schermate, e nella tabella di equivalenza installazione, suono, gioco
  offline, progressione da spento e console per Web, iOS e Android.

### Modificato

- README e `COMANDI.md` aggiornati a salvataggi, battaglie, collegamento, emulatore e PWA.
- La versione 2.0 è uguale in firmware, installer (`web/manifest.json`), service worker
  della PWA e «PokeGotchi finto» di `tools/peer_link`.

### Corretto

- **L'installer web cancellava la partita a ogni flash**, anche senza «Erase device»: il
  firmware era un'immagine unica da 0x0 con 0xFF sul tratto della NVS. Ora sono due pezzi
  (`firmware/boot.bin` a 0x0, `firmware/pokegotchi.bin` a 0xE000) e la NVS non si tocca.
- Installer web, «Carica i progressi»: la richiesta della porta seriale falliva dopo la scelta
  del file («Must be handling a user gesture»). Ora una porta già concessa si riapre da sola,
  altrimenti un pulsante «Scegli la porta e invia» la chiede con un clic nuovo.
- PWA: ogni partita nuova aveva gli stessi geni (e lo stesso caso per shiny e rarità): il
  seme di `random()` saturava a una costante. Ora viene da `crypto.getRandomValues`.
- PWA ed emulatore: l'orologio del gioco era in UTC, un'ora o due indietro rispetto alla
  scheda, che conta in ora locale. Ora è in ora locale, e le date dei salvataggi si leggono
  come sulla scheda. Una partita della PWA già avviata riceve una volta 1–2 ore di
  progressione al primo avvio della versione corretta.
- PWA con `?debug` sul telefono: la console copriva i pulsanti sotto la Pokéball.
- Battaglia: il suono di rifiuto sulla mossa senza PP non si sentiva con la musica (300/200 Hz
  finivano sotto il basso del brano): ora è un'onda stretta, più ronzante. Le mosse esaurite
  hanno il nome sbiadito e barrato, non più solo il fondo grigio che si confondeva con il
  tipo Normale. Nelle targhe livello e «PS» sono grandi come il nome, e lo stato è una
  pillola sul bordo.
- COLLEGAMENTO: un tocco appena fuori da un pulsante chiudeva tutto e spegneva la radio (il
  dito cade di solito 10–20 px sotto il punto mirato), per esempio RIPROVA dopo «Aggiorna il
  firmware». Ora i pulsanti hanno un margine, soprattutto in basso, e si chiude solo dalla
  fascia «tocca: indietro» o scorrendo verso l'alto.
- Squadra della lotta in collegamento: il pet attivo che non può lottare spariva senza
  spiegazione; ora resta grigio con il motivo («dorme», «stanco», «occupato»).
- Collezione: con uno scambio in corso o sospeso LIBERA era rosso e apriva la conferma, che
  poi non faceva nulla. Ora è grigio e dà il suono di rifiuto, come RIPRENDI.
- La fuga per abbandono partiva con un solo tocco sul pulsante scuro: ora chiede conferma
  («Lascialo andare» / «Me ne prendo cura»), come l'addio.
- Pokédex: il pulsante COLLEZIONE è più grande (la griglia sale un po') e si tocca più
  facilmente. Pagina Medaglie: le descrizioni lunghe vanno a capo invece di uscire dalla
  pillola.
- `IMPORT` dalla seriale aspettava la conferma sullo schermo per sempre, anche dopo che
  `salvataggi.py` o l'installer avevano smesso di aspettare: ora la scheda annulla da sola
  dopo 150 s e risponde `ERR SCADUTO`.
- Copie sulla SD datate nel futuro (prove con l'orologio avanti): erano proposte all'avvio
  al posto di quelle vere, e la rotazione toglieva quelle di oggi. Ora non contano tra le
  più recenti e non si tolgono, e la proposta preferisce una copia non futura.
- PWA su iOS: più tocchi veloci fuori dalla tela facevano scattare lo zoom del doppio tocco.
- Installer web: c'è l'icona della pagina (prima la console mostrava un 404 per
  `/favicon.ico`).
- Console seriale: i messaggi ereditati in spagnolo (sprite, miniature, SD, RTC, sensori)
  sono in italiano, e la riserva degli sprite TPK1 non riempie più il log quando manca.
- Memoria: l'heap libero a riposo era sceso da 285 a 245 KB con il collegamento, e in
  collegamento a 195 KiB. Gli oggetti del collegamento, la battaglia e l'elenco dei
  salvataggi stanno ora nella PSRAM, e la radio usa solo il driver ESP-NOW (senza lo stack
  di rete, che restava allocato dopo il primo collegamento): 262 KB a riposo, 226 KiB in
  collegamento, e all'uscita torna quasi tutto (3 KB).
- Console: `AUDIO` mostra anche quanti effetti sono arrivati al motore e l'ultimo; nuovo
  comando di debug `PP <mossa> <n>` per i PP in battaglia.

### Limiti noti della PWA

- Nessun avanzamento in background né notifiche: il tempo si recupera alla riapertura.
- Niente collegamento (non c'è radio): COLLEGA dice «Radio non disponibile».
- Su iOS Safari e l'app sulla Home hanno archivi separati, e un sito non installato può
  perdere i dati: conviene installare subito ed esportare ogni tanto.

## [1.20] - 2026-10-24

### Aggiunto

- **Collegamento tra due schede** via ESP-NOW: scheda → pagina Lotta → «Collega». La radio
  si accende solo lì. Abbinamento sicuro con un codice a 4 cifre uguale sui due schermi e la
  conferma di entrambi (un terzo dispositivo sul canale non può dirottarlo), controllo di
  compatibilità con le due versioni a schermo, ritrasmissione e keepalive. Protocollo in
  `link.h/.cpp` (con test su un canale che perde, duplica e riordina), trasporto in
  `linknow.h/.cpp`, derivato da DylanPDao/TamaPoke (MIT) e corretto. Il nome
  dell'allenatore si sceglie al primo collegamento ed entra nei salvataggi.
- **Battaglie in collegamento** a squadre fino a 3 (il pet attivo e la collezione), con la
  regola Coppa scelta dall'host (livello libero o tutti al 50), menu LOTTA / POKEMON / FUGA,
  cambi e cambio forzato dopo un KO, resa e rivincita. L'host risolve i turni e l'ospite li
  ripete sulla sua copia. Una vittoria conta nelle **vittorie in collegamento** e dà un po'
  di legame al pet attivo se era in squadra; niente allenamento.
- **Scambi di Pokémon** («Scambia»): si offre il pet attivo o uno della collezione (niente
  uova né cerimonie), le due offerte si vedono su entrambi gli schermi (miniatura, livello,
  shiny, allenatore originale) e lo scambio parte quando hanno accettato tutti e due, dopo la
  conferma «Scambiare … con …?» e una copia `…-scambio.pgsave` sulla microSD. Il ricevuto
  tiene specie, livello, shiny, geni, allenamento e nome, riparte con legame 0 ed entra nel
  Pokédex; la scheda mostra «AO: <nome>» e i Progressi contano gli scambi. **Kadabra,
  Machoke, Graveler e Haunter evolvono alla ricezione** (con la cerimonia per il pet attivo,
  con le miniature in collezione). Il protocollo ha un giornale in NVS: uno scambio
  interrotto, anche spegnendo una scheda, non perde né duplica mai un Pokémon e si completa
  ricollegandosi con lo stesso allenatore; l'annullamento manuale resta come ultima risorsa,
  con un avviso.
- **Collezione personale** di 5 posti (dal Pokédex, «Collezione»): dopo un addio o il
  rilascio di una forma finale il pet può restare, congelato. «Riprendi» torna ad allevarlo
  (una volta al giorno), «Libera» lo lascia andare. Fornisce le squadre del collegamento ed
  entra nei salvataggi.
- **Musica di battaglia di Rosso/Blu** (APU del Game Boy emulata e l'interprete del motore
  audio di pret/pokered), con **volume** da 0 a 10 e interruttore **MUSICA** nel nuovo
  pannello Audio delle impostazioni.
- Console: `LINK` (sempre attivo: collegamento, lotta e scambio), `COL`, `AUDIO`, `TIME
  <epoch>` sempre attivo; dopo `DEBUG ON` `COLADD`, `COLSI`, `COLNO`.
- Stringhe nuove in tutte le 8 lingue.

### Modificato

- Orologio: schede ORA e DATA, così la data si corregge anche dallo schermo.
- Testi secondari più contrastati; durante un'evoluzione i gesti sono ignorati.
- Salvataggi: lo stato del pet passa alla versione 4 (collezione, allenatore originale,
  ultimo scambio applicato) e si aggiungono le chiavi `tid`, `tnome`, `lkvit`, `lkscb`,
  `col`, `vol` e `mus`. I `.pgsave` precedenti si importano come sempre.

### Corretto

- Riavvio (Guru Meditation) quando un uovo si schiudeva entro 4 s da una medaglia.

### Interno

- Tutto l'accesso all'hardware passa da `hal.h`; il firmware intero gira nell'emulatore
  desktop SDL2 (`tools/emu`, con lo smoke test di 48 h e due istanze collegate via UDP) e
  nella PWA in WebAssembly (`web/app`, che uscirà con la 2.0).
- `tools/peer_link`: il «PokeGotchi finto» per una seconda scheda ESP32 qualsiasi, che si
  abbina, lotta e scambia da copione, per provare il collegamento con una sola Waveshare.

## [1.19] - 2026-10-22

### Aggiunto

- **Battaglie contro Pokémon selvatici.** Il pulsante «Lotta» nella pagina Combattimento
  della scheda avvia una battaglia 1 contro 1 contro un Pokémon casuale del bioma del pet, di
  livello vicino (da 2 sotto a 1 sopra; niente leggendari né forme evolute prima del loro
  livello). Non si lotta da uovo, nel sonno, in evoluzione, in cerimonia né con l'energia
  sotto 20. Ogni battaglia costa energia e sazietà (la fuga meno); le prime 5 vittorie del
  giorno danno felicità, legame e allenamento di FORZA e DIFESA, una sconfitta toglie un po'
  di felicità. Nessun effetto sul ciclo di vita, PS non persistenti, salvataggio differito.
  Regole e numeri in [`COMANDI.md`](COMANDI.md#battaglia).
- **Schermata di battaglia** (`ui_battle.cpp`, layout derivato da DylanPDao/TamaPoke, MIT):
  avversario in alto a destra, il nostro in basso a sinistra e specchiato, barre PS in stile
  Game Boy, riquadro di testo «tocca per continuare», menu LOTTA / FUGA, griglia 2×2 delle
  mosse con il colore del tipo e i PP. Animazioni PMD Attack/Hurt, KO che affonda sotto la
  pedana, suoni nuovi di attacco, colpo e KO. Una cerimonia che parte chiude la battaglia.
- **Motore di battaglia di Rosso/Blu** (`battle.h/.cpp`, derivato da DylanPDao/TamaPoke,
  MIT): formula del danno di pokered con Speciale, tipi doppi, STAB, brutti colpi, PP e
  Scontro; sonno, congelamento, paralisi, veleno e Tossina, scottatura, confusione,
  tentennamento; mosse a due turni, Fossa e Volo, Avvolgibotta, Ira, Nebbia, barriere, Nube,
  Conversione, Parassiseme e Trasformazione. RNG con seme (battaglie riproducibili), IA della
  CPU facile e difficile, statistiche del pet con la formula di Gen 1 (DV dai geni, bonus
  dall'allenamento, le ultime 4 mosse imparate). Coperto da `test/test_battle.cpp`.
- **Dati di battaglia Gen 1**: i 15 tipi e la tabella dei tipi, la Speciale, mosse e learnset
  di Rosso/Blu (da pret/pokered, `tools/fetch_gen1.py`), i nomi delle mosse in 8 lingue (da
  PokéAPI) e il nuovo header generato `moves.h`.
- **Pagina Combattimento della scheda**: i tipi, la Speciale accanto ad ATK/DEF/VEL e al peso,
  le 4 mosse attuali con i PP, e il pulsante «Lotta» accanto ad «Allena forza».
- Console (dopo `DEBUG ON`): `BATTLE <dex> [lvl]`, una battaglia di prova senza costi né
  premi. `HEALTH` stampa anche la PSRAM libera.
- Stringhe nuove della battaglia e i nomi dei 15 tipi in tutte le 8 lingue.

### Modificato

- **Lo stato del Pokémon si salva in un'unica voce NVS** (il blob `st`, con versione e CRC32)
  scritta in una sola volta: un'interruzione di corrente lascia lo stato vecchio o quello
  nuovo, mai un misto. Al primo avvio della 1.19 le chiavi singole delle versioni precedenti
  migrano da sole nel blob e poi vengono tolte; il percorso di migrazione resta per qualche
  release. Aggiornare dall'installer **senza** «Erase device» mantiene il Pokémon.
- I salvataggi `.pgsave` passano allo schema 2: quelli della 1.17 e della 1.18 si importano
  e migrano, quelli nuovi su una 1.18 chiedono di aggiornare il firmware.
- Dentro il firmware, la UI è una macchina a stati (una schermata alla volta, `goScreen()`) e
  lo sketch è diviso in `ui_*.cpp` e `console.cpp`. Nessun cambiamento visibile, salvo la
  pagina Combattimento della scheda.

## [1.18] - 2026-10-21

### Aggiunto

- **Salvataggi `.pgsave`.** Un file contiene tutte le chiavi del gioco nella NVS
  (creatura, Pokédex, medaglie, serie, lingua, suono), con un'intestazione leggibile
  (versione del firmware e dello schema, data, motivo, riassunto con specie e livello),
  serializzazione little-endian esplicita e CRC32. Il modulo `savefile.h/.cpp` non dipende
  dall'hardware ed è coperto da `test/test_save.cpp`.
- **Importazione sicura**: magic, lunghezza, CRC e schema vengono verificati prima di
  scrivere qualsiasi cosa, e il motivo di un rifiuto compare a schermo. Un file con uno
  schema più recente chiede di aggiornare il firmware. Lo stato attuale viene copiato sulla
  SD prima di sovrascriverlo, e il tempo tra la copia e il ripristino non conta.
- **Copie sulla microSD** in `/save/`, che sopravvivono al riflash con «Erase device»: una
  al giorno a schermo attenuato, una al primo avvio di un firmware diverso (prima che i dati
  vengano migrati), una prima di ogni importazione. Si tengono le 10 più recenti più
  l'ultima di ogni motivo; senza SD le copie vengono saltate e le impostazioni lo segnalano.
- **Impostazioni → Salvataggi**: «Crea salvataggio ora» ed elenco delle copie con data,
  motivo, miniatura e livello, con la conferma del ripristino. All'avvio la scheda propone
  un `/save/importa.pgsave` copiato con un lettore di schede e, con una partita nuova, il
  salvataggio più recente prima della scelta dell'iniziale.
- Comandi seriali sempre attivi `EXPORT` (base64 su una riga), `BACKUP` e
  `IMPORT <base64>`, che scrive solo dopo il SÌ sullo schermo.
- **`tools/salvataggi.py`** con `esporta`, `importa` e `info` (decodifica intestazione e
  chiavi e verifica il CRC, senza scheda).
- **Installer web**: flusso in quattro passi (salva i progressi, flasha, ripristina, sprite)
  e una sezione «Salvataggi» per scaricare e caricare i progressi in ogni momento.
- **[`COMANDI.md`](COMANDI.md)**: ogni gesto, pulsante e comando, schermata per schermata,
  con i diagrammi SVG chiaro/scuro delle zone di tocco generati da `tools/make_diagrams.py`,
  e la sezione «Salvataggi». README: sezioni «Salvataggi» e «Ringraziamenti»; `CREDITS.md`.

### Modificato

- **L'italiano è ora la lingua predefinita** dell'interfaccia (`LANG_DEFAULT`); chi ha già
  una lingua salvata nella NVS la conserva.
- Tutti i commenti del codice, i README e i file `.md` sono stati tradotti in italiano, così come
  la pagina dell'installer web. Anche gli identificatori di `tools/` e i nomi dei test sono
  in italiano.
- **I comandi seriali che modificano la partita** (`SPEC`, `LVL`, `HATCH`, `WIPE`…)
  rispondono `ERR` finché non si invia `DEBUG ON`, che vale fino al riavvio; il reset di
  fabbrica richiede `WIPE YES`.
- **La microSD non viene mai formattata**: se non si monta, la console seriale spiega le
  cause possibili (per esempio exFAT invece di FAT32) e il gioco usa gli sprite in flash.

### Corretto

- **Uno stato NVS rovinato non blocca più il gioco.** `load()` riporta nei limiti specie,
  barre, geni, cacche, finali e medaglie; una specie fuori intervallo diventa un uovo nuovo.

## [1.17] - 2026-09-18

### Aggiunto

- **Il coreano, l'ottava lingua.** Interfaccia completa (85 stringhe), le tre
  lunghezze dei nomi delle medaglie e i 151 nomi delle specie della gen 1, sulla stessa
  struttura `STRINGS[LANG][StrId]` di tutte le altre lingue. La misura del testo
  centralizzata nella 1.15 regge quasi tutto; i quattro punti basati sui byte che una
  riga UTF-8 ha fatto emergere sono elencati sotto Corretto.
- `u8g2_font_unifont_t_korean2` per l'Hangul, scelto per lingua in
  `applyLangFont()` invece che tramite un unico `CJK_FONT`. Il coreano aggiunto
  usa 296 sillabe distinte, tutte in KS X 1001, l'insieme da 2350 sillabe
  che corrisponde alla dimensione di `korean2`; `korean1` ne contiene 478. La copertura è stata verificata
  come appartenenza a KS X 1001 invece di scorrere la tabella del font, e
  l'ASCII nelle stringhe si dà per presente. Non ancora verificato sulla scheda:
  se `CJK_SIZE_DIV` vada bene a `korean2` come al sottoinsieme giapponese.

### Corretto

- **Il dialogo di rilascio poteva tagliare un nome coreano a metà carattere.**
  `renderRelease()` scriveva in `char q[28]`. `"%s 놓아줄까요?"` sono 17 byte
  prima del nome, quindi i nomi di quattro sillabe o più (34 dei 151) non ci
  stavano, e `snprintf` troncava dentro una sequenza da 3 byte, lasciando una coda che il
  font non sa decodificare. Ora è 48; il caso più lungo richiede 33 byte incluso il
  terminatore.
- **Tre punti dimensionavano e posizionavano il testo con `strlen()`**, che conta i
  byte: l'intestazione della scheda delle statistiche, il nome della specie sotto un soprannome e
  l'intestazione del dettaglio della galleria. Su una riga UTF-8 lo scostamento di centraggio e la soglia
  di rimpicciolimento automatico lavoravano su un conteggio di byte invece che su una larghezza. Tutti e tre ora
  passano da `textW()` / `centerX()`. Con il font classico `textW()` restituisce
  `strlen()*6*size`, la stessa espressione di prima, quindi le sei lingue latine
  cadono sulla stessa coordinata alla stessa dimensione, verificato a ogni lunghezza da 1
  a 25.
- **Tre testi giapponesi erano tagliati**, già presenti nella 1.15 e nella 1.16.
  I loro buffer erano dimensionati quando ogni stringa era di un byte per carattere, e
  `snprintf` tronca senza lasciare traccia. Il record del minigioco leggeva `きろく 2`
  con un massimo di 219, perché `char rec[12]` lasciava spazio per una sola cifra. Il banner
  del traguardo di serie non ci stava proprio: `"%u にちれんぞく！"` supera 20 byte con una
  sola cifra, e il taglio cadeva a metà carattere. La riga della serie nella scheda delle
  statistiche perdeva cifre dai 100 giorni in poi. Il dialogo di rilascio corretto sopra era il
  quarto.
- **Ogni testo formattato è ora controllato rispetto al suo buffer, in tutte e otto le
  lingue.** Un nuovo test in `test/test_tools.py` trova ogni
  `snprintf(buf, sizeof(buf), T(...))` nello sketch, legge la dimensione di `buf`,
  e calcola il caso peggiore per lingua dai tipi degli argomenti (5 cifre
  per `%u`/`%d`, 10 per `%lu`, e un massimo dichiarato per ogni `%s`). È ciò che ha
  intercettato i tre sopra, e le avrebbe intercettate tutte e quattro. Per farlo passare
  per tipo e non per valore tipico, altri quattro buffer ricevono un margine di cui non
  avevano ancora avuto bisogno: il conteggio delle medaglie, la riga del livello successivo, il
  conto alla rovescia dell'evoluzione e la riga del profilo.
- **`tools/test_i18n_formats.py` leggeva solo i primi `len(LANGS)` blocchi di
  lingua**, e `LANGS` ne elencava sei, quindi le righe successive venivano saltate senza
  avviso. Ora le elenca tutte e otto, e la docstring annota che deve
  restare completo.
- **`LANG_NAME[LANG_COUNT]` di `test/test_i18n.cpp` aveva sei voci**, meno di
  `LANG_COUNT`, quindi ogni voce oltre la sesta era nulla. Quattro dei test
  costruiscono un'etichetta da `LANG_NAME[lang]` a ogni iterazione, non solo quando un
  controllo fallisce, quindi ogni esecuzione passava un puntatore nullo a `%s`. Ora ne elenca otto.

## [1.16] - 2026-09-09

### Corretto

- **Il giapponese perdeva in silenzio ogni `！` e `？`.** 21 punti esclamativi
  e 4 punti interrogativi — un quarto delle stringhe giapponesi, inclusa la riga
  dell'umore nella schermata principale (`ごきげん！` veniva mostrato come `ごきげん`). La causa è che
  `u8g2_font_unifont_t_japanese1` contiene kana e kanji ma nessuna punteggiatura CJK
  — niente `！？。、「」` — e `Arduino_GFX` non disegna nulla per un glifo che non
  trova, senza nemmeno avanzare il cursore, quindi la perdita è invisibile nel
  sorgente e a schermo. Passato a `japanese3`, l'unico font giapponese U8g2
  che li ha. Stessa famiglia unifont: l'intestazione delle metriche è identica byte per byte e
  tutti i 226 codepoint usati dal firmware hanno bitmap identiche byte per byte, quindi nulla
  si sposta. Costa 102 KB di flash (705 KB → 810 KB, 22% → 25% della partizione
  dell'app); la RAM non cambia.
- **Nidoran♀ e Nidoran♂ comparivano entrambi come ニドラン in giapponese**, rendendo il #29
  e il #32 indistinguibili nel Pokédex. Nessun sottoinsieme unifont di U8g2 contiene
  ♀ (U+2640) o ♂ (U+2642) — né `japanese*`, né `korean*`, né `chinese*` — e
  `Arduino_GFX` non disegna nulla per un glifo che non trova, senza nemmeno
  avanzare il cursore. `gen_names.py` sostituiva già F e M sul percorso
  latino; ora lo fa anche su quello UTF-8, così i nomi si leggono
  ニドランF e ニドランM. Convenzione scelta da @usakomint, che l'ha preferita
  a ニドラン(女)/(男) perché più vicina a un nome di specie e più corta su uno schermo
  piccolo e rotondo.

Entrambi confermati su hardware da @usakomint, che inoltre — senza saperlo —
ha fornito la prova del primo: il `！` mancante era visibile in una
foto che ha pubblicato della schermata principale della v1.15.

## [1.15] - 2026-09-08

### Aggiunto

- **Il giapponese.** Interfaccia, medaglie e tutti i 151 nomi delle specie in katakana (フシギダネ,
  ピカチュウ). Selezionabile dalla schermata delle impostazioni come ogni altra lingua.

### Modificato

- La misura del testo e il posizionamento del cursore sono ora centralizzati (`textW()`,
  `centerX()`, `setCur()`, `setSize()`, `printT()`), al posto di ~180 punti che
  assumevano un byte per carattere e una larghezza fissa di 6 px. Le lingue latine si disegnano
  in modo identico — l'equivalenza è aritmetica, non approssimata. È ciò che
  rende possibile il CJK senza un ramo per lingua a ogni punto di disegno, e
  cinese e coreano richiedono ora solo le loro stringhe.
- Le posizioni delle barre nella scheda delle statistiche e nella riga delle cure derivano dalla larghezza
  dell'etichetta tradotta più larga invece che da coordinate fisse, così restano allineate
  in qualsiasi lingua. Con etichette latine i numeri risultano identici a prima.
- Nuova dipendenza: **U8g2**, solo per i dati dei suoi font CJK.

La gestione del font giapponese, la leggibilità e il layout sono stati tutti messi a punto su
hardware reale da @usakomint, che ha trovato e diagnosticato ogni problema lungo il percorso.

## [1.11] - 2026-09-07

### Corretto

- **Tutto il loop si bloccava per esattamente 1 secondo toccando lo schermo.**
  `touch.getPoint()` restava appeso su un CST9217 addormentato per il timeout
  predefinito di 1000 ms del driver I2C dell'ESP32, che `Wire.setTimeOut(50)` non limita perché
  SensorLib non passa da lì. Più visibile nel minigioco della palla, dove
  i tocchi sono rapidi e due o tre blocchi si sommavano — segnalato come "si blocca per
  2–3 secondi toccando la palla". Una transazione Wire di solo indirizzo prima della lettura
  lo risolve. Misurato su hardware con un gioco equivalente (~470 letture del touch): 5
  blocchi in 60 s prima, 0 dopo. Segnalato da @gingsiro.

## [1.10] - 2026-09-07

### Corretto

- Gli sprite TPK2 con durate dei frame pari a zero facevano girare `pmdFrameAt()` all'infinito,
  bloccando il loop di render. Questi file arrivano via USB, quindi non sono input
  fidato: le durate vengono limitate al caricamento e il ciclo è comunque delimitato.
- `thumbs.bin` veniva mappato in PSRAM senza delimitarne la dimensione né verificare che
  gli offset che contiene puntassero dentro quanto effettivamente letto. Un file troncato
  (plausibile — il trasferimento dura minuti) poteva leggere oltre l'allocazione.
- Il gestore seriale di `PUT` ignorava il valore di ritorno di `f.write()`, quindi una scheda SD piena
  segnalava comunque `DONE` lasciando un file troncato. Prendeva inoltre il
  percorso di destinazione direttamente dalla riga seriale, quindi un `PUT` costruito ad arte poteva scrivere
  ovunque nella scheda; le scritture sono ora confinate a `/mons/`.
- I tocchi appena fuori dalla griglia del Pokédex e dalla tastiera a schermo venivano trattati
  come cella 0. La guardia divideva prima e poi controllava un indice negativo, ma
  la divisione intera tronca verso zero, quindi non ne vedeva mai uno. Su uno schermo rotondo
  quella striscia è esattamente dove cade un dito vicino al bordo.

Tutti e quattro da @ajtudela.

## [1.9] - 2026-09-07

### Corretto

- Tutti i ~13 timer del gioco confrontavano `millis()` con una scadenza assoluta, il che
  dà la risposta sbagliata quando `millis()` va a capo a ~49,7 giorni di uptime: un timer
  poteva restare attivo per sempre, o una cerimonia interrompersi all'istante.
- Il trasferimento degli sprite ripartiva da zero a ogni tentativo. Ora registra
  ogni file confermato, così un ricaricamento dopo una connessione caduta non rifà
  tutti i ~40 MB / ~10 min.
- La fisica del minigioco avanzava per frame invece che per unità di tempo, quindi la palla
  cadeva in modo misurabilmente più lento con uno sprite grande a schermo (Charizard) che con uno
  piccolo (Diglett) — i punteggi migliori non erano confrontabili tra specie.

### Sicurezza

- `esp-web-tools` veniva caricato da un intervallo flottante `@10` senza alcun
  controllo di integrità. Fissato alla 10.4.0 con un hash SRI sha384 verificato.

Tutto da @ajtudela.

## [1.8] - 2026-09-07

### Corretto

- **Il contatore del livello tornava a 0 dopo ~10,6 giorni di gioco.** `level()`
  restituiva `uint8_t` e, a 60 minuti per livello, il livello è il numero di ore —
  quindi andava a capo a 256. Raggiungibile nel gioco normale, dato che "restare insieme"
  rimanda l'addio all'infinito. Ora `uint16_t`, con limite a 999.
- Due troncamenti correlati trovati durante la correzione: `calcStat()` prendeva il livello come
  `uint8_t`, quindi le statistiche sarebbero comunque andate in overflow; e `canEvolveNow()` confrontava
  con `(uint8_t)(evolveLevel + careMistakes)`, che andava a capo con molti errori di cura
  e faceva evolvere in anticipo. La scheda delle statistiche calcolava già quella soglia come `int`,
  quindi lo schermo e il controllo effettivo potevano non concordare.

Diagnosticato da @ajtudela.

## [1.7] - 2026-09-07

### Corretto

- **L'interfaccia non aveva accenti, e il presupposto alla base era sbagliato.** Il codice
  affermava in due punti che il font non ne aveva. In realtà `glcdfont.h` è una tabella
  CP437 completa da 256 glifi e `write()` non filtra i byte ≥ 0x80. Lo spagnolo ora
  ha le sue tilde e la ñ, il tedesco le sue dieresi, il francese i suoi accenti. I caratteri sono
  scritti come singoli byte in ottale, mai in UTF-8, perché l'interfaccia centra con
  `strlen(t) * 6`. Sono state toccate solo le stringhe la cui lunghezza non cambiava.
  Le vocali maiuscole accentate diverse da É non sono in CP437, quindi `EVOLUCION` e
  `PROGRES` restano senza accento, così come le ã/õ portoghesi.
- Grammatica tedesca, dallo stesso contributore: "Wähle deinen Starter" (accusativo),
  "Leb wohl" in due parole, "Entwicklung" invece di "Entwickelt", e altro.

Scoperto da @danielberndt.

## [1.6] - 2026-09-07

### Corretto

- **Gli effetti sonori brevi erano inudibili.** L'amplificatore veniva acceso 8 ms
  prima di ogni effetto, molto meno di quanto serva all'NS4150B per stabilizzarsi, così un
  tocco da 35 ms finiva prima che l'altoparlante si svegliasse. Il jingle di avvio si salvava perché dura
  ~440 ms — ecco perché il suono sembrava funzionare all'avvio e da nessun'altra parte. L'
  amplificatore ora resta alimentato finché il suono è attivo e il Pokémon è sveglio, come nel riferimento di
  Waveshare. Note e ampiezza restano invariate: non altera il
  sound design, lo rende semplicemente udibile.

Diagnosticato da @djyf1.

## [1.5] - 2026-08-19

### Aggiunto

- Nomi dei Pokémon localizzati in francese e tedesco (Bulbizarre, Bisasam…). Solo
  queste due lingue differiscono in gen 1; spagnolo, italiano e portoghese usano ufficialmente i
  nomi inglesi. I nomi provengono da PokéAPI tramite `tools/gen_names.py`.

## [1.4] - 2026-08-07

### Corretto

- **Il legame era matematicamente bloccato.** Poteva guadagnare al massimo 12 punti al giorno, ma
  un singolo errore di cura ne costava 3 e poteva ripetersi ogni 30 minuti — fino a 144 al
  giorno. Con le cacche che consumavano l'igiene, gli errori si concatenavano e il legame non si riprendeva mai.
  Due giocatori lo hanno segnalato in modo indipendente bloccato esattamente a 10, che è il
  tetto del primo giorno di 13 meno un errore. Limite giornaliero 8 → 20, penalità 3 → 1,
  attesa 30 → 60 min.

## [1.3] - 2026-08-07

### Corretto

- **L'uovo si schiudeva da solo durante la scelta dell'iniziale.** Il tempo di gioco continuava a scorrere
  mentre la schermata dell'iniziale era aperta, quindi impiegare più di 3 minuti a scegliere
  significava che l'uovo si schiudeva nella specie già sorteggiata — niente animazione dell'uovo, e
  la scelta del giocatore ignorata. Tre segnalazioni della community coincidevano (Pikachu due volte,
  Eevee una). Ora il tempo si congela finché non si sceglie un iniziale.

## [1.2] - 2026-06

### Aggiunto

- Versione del firmware mostrata in fondo alla schermata dell'orologio/impostazioni.
