#pragma once

// Pin ufficiali della Waveshare ESP32-S3-Touch-AMOLED-1.75
// Fonte: github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75 (libraries/Mylibrary/pin_config.h)

#define XPOWERS_CHIP_AXP2101

// Schermo AMOLED 466x466, driver CO5300 via QSPI
#define LCD_SDIO0 4
#define LCD_SDIO1 5
#define LCD_SDIO2 6
#define LCD_SDIO3 7
#define LCD_SCLK 38
#define LCD_CS 12
#define LCD_RESET 39
#define LCD_WIDTH 466
#define LCD_HEIGHT 466

// Touch capacitivo CST9217 via I2C
#define IIC_SDA 15
#define IIC_SCL 14
#define TP_INT 11
#define TP_RESET 40

// Audio ES8311. NOTA: l'MCLK reale è GPIO42 (verificato sulla scheda con il progetto
// PlaneRadar2.0); il 16 riportato era errato. Comunque il codec si configura con clock
// derivato dal BCLK, quindi l'MCLK conta poco.
#define I2S_MCK_IO 42
#define I2S_BCK_IO 9
#define I2S_DI_IO 10
#define I2S_WS_IO 45
#define I2S_DO_IO 8
#define PA 46

// Slot TF (non ancora usato)
#define SDMMC_CLK 2
#define SDMMC_CMD 1
#define SDMMC_DATA 3
#define SDMMC_CS 41
