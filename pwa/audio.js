// AudioWorklet della PWA: suona i campioni che il motore di musica.h,
// nel WASM sul thread principale, produce per lui. Niente SharedArrayBuffer (quindi niente
// header COOP/COEP): i blocchi arrivano con postMessage come Float32Array trasferiti e
// finiscono in un buffer ad anello.
//   - pagina -> worklet: un Float32Array di campioni mono, già con il volume;
//   - worklet -> pagina: ogni RAPPORTO campioni { consumati, vuoti, picco }, i contatori
//     dall'avvio: la pagina ne ricava quanti campioni ci sono in coda e la riempie
//     (app.js, riempiAudio), così la produzione segue il clock del contesto audio.
// Se l'anello si svuota esce silenzio (vuoti conta quei campioni); se trabocca si scartano
// i campioni più vecchi.
'use strict';

const CAPIENZA = 1 << 15;  // ~0,7 s a 48 kHz, molto più della coda che la pagina tiene
const RAPPORTO = 1024;     // campioni tra un rapporto e l'altro (8 quanti da 128)

class PokeGotchiAudio extends AudioWorkletProcessor {
  constructor() {
    super();
    this.anello = new Float32Array(CAPIENZA);
    this.leggi = 0;      // posizioni assolute: l'indice è % CAPIENZA
    this.scrivi = 0;
    this.consumati = 0;  // campioni suonati dall'anello
    this.vuoti = 0;      // campioni di silenzio per anello vuoto
    this.picco = 0;      // il massimo |campione| suonato
    this.daRapporto = 0;
    this.port.onmessage = (e) => this.metti(e.data);
    this.port.postMessage({ consumati: 0, vuoti: 0, picco: 0 });  // la prima richiesta
  }

  metti(blocco) {
    for (let i = 0; i < blocco.length; i++) this.anello[(this.scrivi + i) % CAPIENZA] = blocco[i];
    this.scrivi += blocco.length;
    const via = this.scrivi - this.leggi - CAPIENZA;
    if (via > 0) {  // si contano come suonati: la pagina misura la coda da consumati
      this.leggi += via;
      this.consumati += via;
    }
  }

  process(_ingressi, uscite) {
    const canali = uscite[0];
    const n = canali[0].length;
    const out = canali[0];
    const c = Math.min(n, this.scrivi - this.leggi);
    for (let i = 0; i < c; i++) {
      const v = this.anello[(this.leggi + i) % CAPIENZA];
      out[i] = v;
      if (v > this.picco) this.picco = v;
      else if (-v > this.picco) this.picco = -v;
    }
    out.fill(0, c);
    this.leggi += c;
    this.consumati += c;
    this.vuoti += n - c;
    for (let k = 1; k < canali.length; k++) canali[k].set(out);  // mono su tutte le uscite
    this.daRapporto += n;
    if (this.daRapporto >= RAPPORTO) {
      this.daRapporto = 0;
      this.port.postMessage({ consumati: this.consumati, vuoti: this.vuoti, picco: this.picco });
    }
    return true;
  }
}

registerProcessor('pokegotchi-audio', PokeGotchiAudio);
