#pragma once
#include <Arduino.h>

// RTC PCF85063: ora persistente finché la scheda ha alimentazione
bool rtcBegin();
uint32_t rtcEpoch();             // secondi unix; 0 se l'RTC non è valido
void rtcSetEpoch(uint32_t e);

// PMU AXP2101: stato della batteria
bool batBegin();
void pmuEnablePanel();           // accende BLDO1 (OLED VDD 3.3V); chiamare prima di gfx->begin()
int batPercent();                // 0-100, -1 se non c'è la batteria collegata
bool batCharging();
int batMillivolts();               // tensione della batteria in mV, 0 se non c'è lettura
bool usbPresent();

// pulsante PWR dell'AXP2101: pressione lunga 4s = spegnimento fisico (l'RTC resta attivo);
// la pressione breve la cattura il firmware (schermo on/off)
void pwrSetup();
bool pwrShortPressed();  // da interrogare nel loop
