# Emulatore su PC: il firmware intero, con lo sketch trattato come .cpp.
#   make        -> build/pokegotchi (serve SDL2: sdl2-config o pkg-config sdl2)
#   make smoke  -> 48 h di gioco accelerate senza finestra (e senza suono), aprendo ogni schermata
#   make musica -> un WAV per brano di battaglia in build/musica/ (musica.h, 32 kHz)
#   make prova  -> la prova di fattibilità della grafica (build/prova_gfx.png)
#   make web    -> la PWA con em++: la lancia tools/build_pwa.sh
# Le librerie sono quelle installate da arduino-cli (stesse versioni del firmware).
CXX      ?= g++
CC       ?= gcc
CXXFLAGS ?= -std=c++17 -O1 -g -Wall -Wno-unused-parameter -Wno-format-security -Wno-format-truncation
CFLAGS   ?= -O1 -g
ARDUINO_USER ?= $(shell arduino-cli config get directories.user 2>/dev/null || echo $(HOME)/Arduino)
GFX_DIR  ?= $(ARDUINO_USER)/libraries/GFX_Library_for_Arduino/src
U8G2_DIR ?= $(ARDUINO_USER)/libraries/U8g2/src
BUILD    := build
ROOT     := ../..

# i sottoinsiemi U8g2 usati dal firmware (CJK_FONT_JA/KO in ui_common.cpp)
FONTS    := u8g2_font_unifont_t_japanese3 u8g2_font_unifont_t_korean2

GFX_SRC  := $(GFX_DIR)/Arduino_G.cpp $(GFX_DIR)/Arduino_GFX.cpp $(GFX_DIR)/canvas/Arduino_Canvas.cpp
# shim/ prima di test/shim: Arduino.h è quello dell'emulatore, Preferences.h quello dei test
INCLUDES := -I shim -I $(ROOT)/test/shim -I $(GFX_DIR) -I $(U8G2_DIR)
SDL_CFLAGS := $(shell sdl2-config --cflags 2>/dev/null || pkg-config --cflags sdl2 2>/dev/null)
SDL_LIBS   := $(shell sdl2-config --libs 2>/dev/null || pkg-config --libs sdl2 2>/dev/null)

# il firmware: tutto tranne i driver della scheda (hal_esp32*, rtcbat, audio)
FW_SRC   := $(ROOT)/console.cpp $(ROOT)/pet.cpp $(ROOT)/i18n.cpp $(ROOT)/savefile.cpp \
            $(ROOT)/savesd.cpp $(ROOT)/sdmon.cpp $(ROOT)/battle.cpp $(ROOT)/ritratto.cpp \
            $(ROOT)/apu.cpp $(ROOT)/musica.cpp $(ROOT)/link.cpp $(ROOT)/linkbattaglia.cpp $(ROOT)/linkscambio.cpp $(ROOT)/moto.cpp $(ROOT)/ota.cpp \
            $(wildcard $(ROOT)/ui_*.cpp)
EMU_SRC  := emu.cpp hal_host.cpp tocco_host.cpp arduino_host.cpp png.cpp $(ROOT)/test/shim/preferences.cpp
FW_DEPS  := $(wildcard $(ROOT)/*.h $(ROOT)/*.cpp) $(ROOT)/PokeGotchi.ino $(wildcard shim/*.h) \
            $(ROOT)/test/shim/Preferences.h

.PHONY: all prova smoke musica web clean
all: $(BUILD)/pokegotchi

# u8g2_fonts.c pesa 40 MB: si estraggono solo le definizioni dei font elencati
$(BUILD)/u8g2_font_scelti.c: $(U8G2_DIR)/clib/u8g2_fonts.c Makefile
	@mkdir -p $(BUILD)
	@echo '#include "u8g2.h"' > $@
	@for f in $(FONTS); do \
	  awk -v n="$$f" 'index($$0, "const uint8_t " n "[") == 1 {p = 1} p {print} p && /;[ \t]*$$/ {exit}' $< >> $@; \
	done

$(BUILD)/u8g2_font_scelti.o: $(BUILD)/u8g2_font_scelti.c
	$(CC) $(CFLAGS) -I $(U8G2_DIR)/clib -c -o $@ $<

$(BUILD)/prova_gfx: prova_gfx.cpp png.cpp png.h arduino_host.cpp $(BUILD)/u8g2_font_scelti.o $(wildcard shim/*.h)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ prova_gfx.cpp png.cpp arduino_host.cpp $(GFX_SRC) \
	  $(BUILD)/u8g2_font_scelti.o

# la libreria grafica si compila una volta sola
$(BUILD)/gfx.a: $(GFX_SRC) $(wildcard shim/*.h)
	@mkdir -p $(BUILD)/gfx
	cd $(BUILD)/gfx && for f in $(GFX_SRC); do \
	  $(CXX) $(CXXFLAGS) -I $(abspath shim) -I $(abspath $(ROOT)/test/shim) -I $(GFX_DIR) -I $(U8G2_DIR) -c $$f || exit 1; done
	$(AR) rcs $@ $(BUILD)/gfx/*.o

# lo sketch come .cpp (-x c++), poi il resto
$(BUILD)/pokegotchi: $(EMU_SRC) emu.h png.h $(FW_DEPS) $(BUILD)/gfx.a $(BUILD)/u8g2_font_scelti.o
	@if [ -z "$(SDL_LIBS)" ]; then echo "SDL2 manca: sudo apt install libsdl2-dev / brew install sdl2"; exit 1; fi
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(SDL_CFLAGS) -o $@ -x c++ $(ROOT)/PokeGotchi.ino -x none \
	  $(FW_SRC) $(EMU_SRC) $(BUILD)/gfx.a $(BUILD)/u8g2_font_scelti.o $(SDL_LIBS)

# 48 h accelerate senza finestra: SD e NVS di prova in build/smoke, foto in build/smoke/foto.
# Gli sprite sono quelli di tools/sdcard/mons se ci sono (python3 tools/pack_pmd.py, fuori dal
# repository); senza, oppure con SPRITE=no, il gioco usa gli sprite di riserva.
SPRITE ?= si
smoke: $(BUILD)/pokegotchi
	rm -rf $(BUILD)/smoke && mkdir -p $(BUILD)/smoke/sd $(BUILD)/smoke/foto
	@if [ "$(SPRITE)" = si ] && [ -d ../sdcard/mons ]; then ln -s $(abspath ../sdcard/mons) $(BUILD)/smoke/sd/mons; \
	  else echo "smoke senza sprite di SpriteCollab"; mkdir -p $(BUILD)/smoke/sd/mons; fi
	./$(BUILD)/pokegotchi --headless --muto --fast 500 --seed 1 --epoch 1767268800 --ota-ultima 3.1.0 \
	  --sd $(BUILD)/smoke/sd --nvs $(BUILD)/smoke/nvs.txt --copione smoke.txt > $(BUILD)/smoke/log.txt
	@echo "smoke OK: foto in $(BUILD)/smoke/foto, uscita in $(BUILD)/smoke/log.txt"

# i brani suonati dal motore, da ascoltare (solo apu.cpp e musica.cpp, senza SDL)
$(BUILD)/musica_wav: musica_wav.cpp $(ROOT)/apu.cpp $(ROOT)/musica.cpp $(ROOT)/apu.h $(ROOT)/musica.h \
                     $(ROOT)/musica_dati.h $(ROOT)/sfx_note.h
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -O2 $(INCLUDES) -o $@ musica_wav.cpp $(ROOT)/apu.cpp $(ROOT)/musica.cpp

musica: $(BUILD)/musica_wav
	@mkdir -p $(BUILD)/musica
	./$(BUILD)/musica_wav $(BUILD)/musica

# la PWA: lo stesso firmware con pwa/hal_web.cpp al posto di emu.cpp e hal_host.cpp.
# tools/build_pwa.sh passa CXX=em++ CC=emcc AR=emar BUILD=build/web
WEB_SRC     := $(ROOT)/pwa/hal_web.cpp tocco_host.cpp arduino_host.cpp $(ROOT)/test/shim/preferences.cpp
# FS: la SD in memoria (MEMFS), che sprite.js e salvataggi.js riempiono
WEB_LDFLAGS := -sALLOW_MEMORY_GROWTH=1 -sSTACK_SIZE=1MB -sENVIRONMENT=web -sFORCE_FILESYSTEM=1 \
               -sEXPORTED_RUNTIME_METHODS=ccall,HEAPU8,UTF8ToString,FS
web: $(BUILD)/pokegotchi.js

# em++ non accetta -x c++ sul .ino: lo sketch entra con un #include
$(BUILD)/sketch.cpp: Makefile
	@mkdir -p $(BUILD)
	echo '#include "$(abspath $(ROOT)/PokeGotchi.ino)"' > $@

$(BUILD)/pokegotchi.js: $(BUILD)/sketch.cpp $(WEB_SRC) emu.h $(FW_DEPS) $(BUILD)/gfx.a $(BUILD)/u8g2_font_scelti.o
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $(BUILD)/sketch.cpp \
	  $(FW_SRC) $(WEB_SRC) $(BUILD)/gfx.a $(BUILD)/u8g2_font_scelti.o $(WEB_LDFLAGS)

prova: $(BUILD)/prova_gfx
	./$(BUILD)/prova_gfx $(BUILD)/prova_gfx.png

clean:
	rm -rf $(BUILD)
