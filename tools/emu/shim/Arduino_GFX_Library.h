// Sostituisce l'header unico di Arduino_GFX, che include tutti i bus e tutti i pannelli
// (SPI, Wire, registri ESP32...). Sul PC servono solo la classe base e la tela: il
// pannello vero (QSPI + CO5300) resta fuori e il fotogramma esce da Arduino_Canvas::flush().
#pragma once
#include <Arduino_GFX.h>
#include <canvas/Arduino_Canvas.h>
