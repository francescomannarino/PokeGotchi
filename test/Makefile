# Test della logica del firmware sul PC (senza scheda).
#   make                -> compila ed esegue
#   make FILTER=evolve  -> solo i test il cui nome contiene "evolve"
CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O1 -g -Wall -Wextra -Wno-unused-parameter \
            -fno-omit-frame-pointer
LDFLAGS  ?=
SRC_DIR  := ..
BUILD    := build
BIN      := $(BUILD)/pokegotchi_tests

SOURCES := main.cpp shim/shim.cpp shim/preferences.cpp test_pet.cpp test_dex.cpp test_i18n.cpp test_save.cpp test_battle.cpp \
           test_musica.cpp test_link.cpp test_linkbattaglia.cpp test_linkscambio.cpp test_moto.cpp test_ota.cpp $(SRC_DIR)/apu.cpp $(SRC_DIR)/musica.cpp $(SRC_DIR)/link.cpp \
           $(SRC_DIR)/pet.cpp $(SRC_DIR)/i18n.cpp $(SRC_DIR)/savefile.cpp $(SRC_DIR)/battle.cpp \
           $(SRC_DIR)/ritratto.cpp $(SRC_DIR)/linkbattaglia.cpp $(SRC_DIR)/linkscambio.cpp $(SRC_DIR)/moto.cpp $(SRC_DIR)/ota.cpp

.PHONY: all run clean asan
all: run

$(BIN): $(SOURCES) framework.h shim/Arduino.h shim/Preferences.h $(SRC_DIR)/dex.h $(SRC_DIR)/moves.h $(SRC_DIR)/battle.h $(SRC_DIR)/pet.h $(SRC_DIR)/ritratto.h \
        $(SRC_DIR)/apu.h $(SRC_DIR)/musica.h $(SRC_DIR)/link.h $(SRC_DIR)/musica_dati.h $(SRC_DIR)/sfx_note.h \
        $(SRC_DIR)/linkbattaglia.h $(SRC_DIR)/linkscambio.h $(SRC_DIR)/moto.h $(SRC_DIR)/ota.h canale_link.h
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -I shim -o $@ $(SOURCES) $(LDFLAGS)

run: $(BIN)
	@./$(BIN) $(FILTER)

# la stessa suite sotto AddressSanitizer + UBSan: scova overflow e indici sbagliati
asan: CXXFLAGS += -fsanitize=address,undefined -fno-sanitize-recover=all
asan: BIN := $(BUILD)/pokegotchi_tests_asan
asan:
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -I shim -o $(BUILD)/pokegotchi_tests_asan $(SOURCES) $(LDFLAGS)
	@./$(BUILD)/pokegotchi_tests_asan $(FILTER)

clean:
	rm -rf $(BUILD)
