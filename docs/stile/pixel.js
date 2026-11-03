// Testo in pixel con il font 5x7 dello schermo della scheda: ogni elemento con la classe
// "px" diventa un SVG di quadratini (vettoriale: nitido a ogni zoom e in stampa).
// Serve font5x7.js prima di questo file (lo scrive `python3 tools/font5x7.py js`).
//
//   <span class="px" data-scala="4" data-ombra="#1d1d2c">TITOLO</span>
//     data-scala   lato di un pixel in px CSS (predefinito 3)
//     data-ombra   colore dell'ombra, spostata di un pixel in basso a destra (facoltativa)
//     "|" nel testo va a capo; con la classe "centra" le righe si centrano tra loro
// Il colore è quello del testo (currentColor). Il testo resta per i lettori di schermo.
(function () {
  const font = Uint8Array.from(FONT5X7.match(/../g), (h) => parseInt(h, 16));
  // le lettere accentate nella tabella CP437 del font; il resto fuori dall'ASCII diventa "?"
  const CP437 = {
    'à': 0x85, 'è': 0x8a, 'é': 0x82, 'ì': 0x8d, 'ò': 0x95, 'ù': 0x97, 'É': 0x90,
    'À': 0x41, 'È': 0x45, 'Ì': 0x49, 'Ò': 0x4f, 'Ù': 0x55, '’': 0x27, '…': 0x2e, '·': 0xfa,
    '♥': 0x03, '★': 0x2a, '→': 0x1a, '←': 0x1b,
  };
  const codice = (c) => (c in CP437 ? CP437[c] : c.charCodeAt(0) < 128 ? c.charCodeAt(0) : 0x3f);
  const NS = 'http://www.w3.org/2000/svg';

  function percorso(righe, larghe, s, dx, dy) {
    let d = '';
    righe.forEach((riga, r) => {
      const ox = dx + Math.floor((larghe - riga.length) * 6 / 2) * s * (righe.centra ? 1 : 0);
      [...riga].forEach((c, i) => {
        const b = codice(c);
        for (let x = 0; x < 5; x++) {
          const col = font[b * 5 + x];
          for (let y = 0; y < 7; y++) {
            if ((col >> y) & 1) d += `M${ox + (i * 6 + x) * s} ${dy + (r * 9 + y) * s}h${s}v${s}h-${s}z`;
          }
        }
      });
    });
    return d;
  }

  function pixelTesto(el) {
    if (el.dataset.px === 'fatto') return;  // già disegnato (una pagina spostata)
    el.dataset.px = 'fatto';
    const testo = el.textContent.trim().replace(/\s+/g, ' ');
    if (!testo) return;
    const s = Number(el.dataset.scala || 3);
    const righe = testo.split('|').map((r) => r.trim());
    righe.centra = el.classList.contains('centra');
    const larghe = Math.max(...righe.map((r) => r.length));
    const o = el.dataset.ombra ? s : 0;
    const w = (larghe * 6 - 1) * s + o, h = (righe.length * 9 - 2) * s + o;
    const svg = document.createElementNS(NS, 'svg');
    svg.setAttribute('viewBox', `0 0 ${w} ${h}`);
    svg.setAttribute('width', w);
    svg.setAttribute('height', h);
    svg.setAttribute('aria-hidden', 'true');
    if (o) {
      const ombra = document.createElementNS(NS, 'path');
      ombra.setAttribute('d', percorso(righe, larghe, s, o, o));
      ombra.setAttribute('fill', el.dataset.ombra);
      svg.appendChild(ombra);
    }
    const p = document.createElementNS(NS, 'path');
    p.setAttribute('d', percorso(righe, larghe, s, 0, 0));
    p.setAttribute('fill', 'currentColor');
    svg.appendChild(p);
    const leggibile = document.createElement('span');
    leggibile.className = 'px-testo';
    leggibile.textContent = righe.join(' ');
    el.textContent = '';
    el.append(svg, leggibile);
  }

  window.pixelTesto = pixelTesto;
  document.querySelectorAll('.px').forEach(pixelTesto);
})();
