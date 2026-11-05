// Esegue web/pmd.js in Node sui file di una cartella, per TestPmdJs (test/test_tools.py):
//
//   node test/pmd_node.mjs specie CARTELLA USCITA [idle]   # PGK2 di una specie
//   node test/pmd_node.mjs miniature ELENCO.json USCITA      # thumbs.bin
//
// ELENCO.json: le 151 cartelle delle specie, convertite solo con l'Idle come fa la PWA.
import { readFile, writeFile } from 'node:fs/promises';
import { convertiSpecie, miniature } from '../web/pmd.js';

const prendiDa = (cartella) => async (file) => {
  try {
    return new Uint8Array(await readFile(`${cartella}/${file}`));
  } catch (e) {
    if (e.code === 'ENOENT') return null;
    throw e;
  }
};

const [cmd, ...arg] = process.argv.slice(2);
if (cmd === 'specie') {
  const [cartella, uscita, modo] = arg;
  await writeFile(uscita, await convertiSpecie(prendiDa(cartella), { soloIdle: modo === 'idle' }));
} else if (cmd === 'miniature') {
  const [elenco, uscita] = arg;
  const cartelle = JSON.parse(await readFile(elenco, 'utf8'));
  const idle = [];
  for (const c of cartelle) idle.push(await convertiSpecie(prendiDa(c), { soloIdle: true }));
  await writeFile(uscita, miniature(idle));
} else {
  console.error('uso: pmd_node.mjs specie|miniature ...');
  process.exit(2);
}
