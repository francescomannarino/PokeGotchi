// Impaginatore del libretto: versa i blocchi di #sorgente in pagine A5 di altezza fissa
// (.pagina), con testata, numero di pagina, apertura di ogni capitolo, indice con i numeri
// veri, pagine di appunti fino a un multiplo di 4 (per la stampa a libretto) e retro.
// I blocchi non si spezzano, tranne liste e tabelle, che continuano nella pagina dopo; un
// titolo (h3, h4) non resta da solo in fondo. Alla fine <body> ha data-pagine e data-avvisi
// (i blocchi che non stanno in una pagina intera), che tools/libretto.py controlla.
(function () {
  const TESTATA = 'PokeGotchi · Il libretto del giocatore';
  const libro = document.getElementById('libro');
  const sorgente = document.getElementById('sorgente');
  const avvisi = [];
  const voci = [];  // l'indice: { etichetta, titolo, pagina, appendice }
  let numero = 0;

  const el = (tag, classe, html) => {
    const e = document.createElement(tag);
    if (classe) e.className = classe;
    if (html !== undefined) e.innerHTML = html;
    return e;
  };
  const pixel = (radice) => radice.querySelectorAll('.px').forEach(window.pixelTesto);
  const trabocca = (corpo) => corpo.scrollHeight > corpo.clientHeight + 1;
  const descrivi = (b) => `${b.tagName.toLowerCase()} «${b.textContent.trim().slice(0, 50)}»`;

  function pagina(classi) {
    numero++;
    const p = el('div', `pagina ${classi} ${numero % 2 ? 'dispari' : 'pari'}`);
    p.dataset.numero = numero;
    libro.appendChild(p);
    return p;
  }

  // pagina con testata, corpo e numero; con `cap` e `apertura` la fascia del capitolo
  function paginaTesto(cap, apertura, classi = '') {
    const p = pagina(`${apertura ? 'apertura' : 'normale'} ${classi}`);
    const testata = el('header', 'testata');
    testata.append(el('span', '', TESTATA), el('span', 'capitolo-px', cap ? cap.etichetta : ''));
    p.appendChild(testata);
    if (apertura) {
      const f = el('div', 'fascia');
      const righe = cap.titolo.split('|');
      const scala = Math.max(...righe.map((r) => r.length)) > 18 ? 3 : 4;
      f.innerHTML = `${cap.etichetta ? '<div class="numero-cap px" data-scala="2"></div>' : ''}
        <h2 class="titolo-cap px" data-scala="${scala}" data-ombra="#a3230a"></h2>
        ${cap.sotto ? '<div class="sotto"></div>' : ''}`;
      if (cap.etichetta) f.querySelector('.numero-cap').textContent = cap.etichetta;
      f.querySelector('h2').textContent = cap.titolo;
      if (cap.sotto) f.querySelector('.sotto').textContent = cap.sotto;
      p.appendChild(f);
    }
    const corpo = el('div', 'corpo');
    p.appendChild(corpo);
    const piede = el('footer', 'piede');
    piede.appendChild(el('span', 'numero px', String(numero))).dataset.scala = '2';
    p.appendChild(piede);
    pixel(p);
    return corpo;
  }

  // liste e tabelle: quanti elementi stanno nella pagina, il resto in un clone che continua
  function dividi(blocco, stato) {
    let figli, nuovo, dentro;
    if (blocco.matches('ul, ol')) {
      figli = [...blocco.children];
      nuovo = () => blocco.cloneNode(false);
      dentro = (c) => c;
    } else if (blocco.matches('table')) {
      figli = [...blocco.tBodies[0].rows];
      nuovo = () => {
        const t = blocco.cloneNode(false);
        if (blocco.tHead) t.appendChild(blocco.tHead.cloneNode(true));
        t.appendChild(document.createElement('tbody'));
        return t;
      };
      dentro = (c) => c.tBodies[0];
    } else {
      return false;
    }
    const parte = nuovo();
    stato.corpo.appendChild(parte);
    let k = 0;
    for (; k < figli.length; k++) {
      dentro(parte).appendChild(figli[k]);
      if (trabocca(stato.corpo)) {
        dentro(parte).removeChild(figli[k]);
        break;
      }
    }
    if (k === 0) {
      stato.corpo.removeChild(parte);
      return false;
    }
    if (k === figli.length) return true;
    const resto = nuovo();
    if (blocco.matches('ol')) resto.start = (blocco.start || 1) + k;
    for (const f of figli.slice(k)) dentro(resto).appendChild(f);
    stato.corpo = paginaTesto(stato.cap, false);
    metti(resto, stato);
    return true;
  }

  function metti(blocco, stato) {
    stato.corpo.appendChild(blocco);
    if (!trabocca(stato.corpo)) return;
    stato.corpo.removeChild(blocco);
    if (!blocco.matches('table') && dividi(blocco, stato)) return;
    const titolo = stato.corpo.lastElementChild;
    const portaTitolo = titolo && titolo.matches('h3, h4') && stato.corpo.children.length > 1;
    stato.corpo = paginaTesto(stato.cap, false);
    if (portaTitolo) stato.corpo.appendChild(titolo);
    stato.corpo.appendChild(blocco);
    if (!trabocca(stato.corpo)) return;
    stato.corpo.removeChild(blocco);
    if (dividi(blocco, stato)) return;
    stato.corpo.appendChild(blocco);
    avvisi.push(`non sta in una pagina: ${descrivi(blocco)} (pagina ${numero})`);
  }

  function versa(sezione, cap) {
    const stato = { cap, corpo: paginaTesto(cap, Boolean(cap && cap.titolo)) };
    for (const b of [...sezione.children]) metti(b, stato);
  }

  function speciale(sezione, classe) {
    const p = pagina(classe);
    p.append(...sezione.childNodes);
    pixel(p);
  }

  function impagina() {
    let capitoli = 0, appendici = 0, indice = null;
    for (const s of [...sorgente.children]) {
      if (s.matches('.copertina')) {
        speciale(s, 'copertina');
      } else if (s.matches('.retro')) {
        // pagine di appunti fino a un multiplo di 4, contando anche il retro
        while ((numero + 1) % 4 !== 0) {
          const corpo = paginaTesto(null, false, 'appunti');
          corpo.innerHTML = '<h3>Appunti dell\'allenatore</h3><div class="righe"></div>';
        }
        speciale(s, 'retro');
      } else if (s.matches('.indice')) {
        indice = paginaTesto({ etichetta: '', titolo: s.dataset.titolo }, true, 'indice');
      } else if (s.matches('.capitolo')) {
        const appendice = s.matches('.appendice');
        const etichetta = appendice
          ? `APPENDICE ${String.fromCharCode(65 + appendici++)}`
          : `CAPITOLO ${String(++capitoli).padStart(2, '0')}`;
        const cap = { etichetta, titolo: s.dataset.titolo, sotto: s.dataset.sotto };
        voci.push({ etichetta, appendice, titolo: s.dataset.indice || cap.titolo.replace(/\|/g, ' '), pagina: numero + 1 });
        versa(s, cap);
      } else {
        versa(s, null);  // pagine di solo testo (l'introduzione)
      }
    }
    if (indice) {
      const ol = el('ol');
      for (const v of voci) {
        const li = el('li', v.appendice ? 'appendice' : '');
        li.append(el('span', 'n', v.appendice ? v.etichetta.slice(-1) : String(Number(v.etichetta.slice(-2)))),
                  el('span', 't'), el('span', 'riempi'), el('span', 'p', String(v.pagina)));
        li.querySelector('.t').textContent = v.titolo;
        ol.appendChild(li);
      }
      indice.appendChild(ol);
      if (trabocca(indice)) avvisi.push('l\'indice non sta in una pagina');
    }
    sorgente.remove();
    document.body.dataset.pagine = numero;
    document.body.dataset.avvisi = JSON.stringify(avvisi);
    if (avvisi.length) console.warn(avvisi.join('\n'));
  }

  if (document.readyState === 'complete') impagina();
  else window.addEventListener('load', impagina);
})();
