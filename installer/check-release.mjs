import { readFile } from 'node:fs/promises';
import { basename } from 'node:path';
import { consoles, readRelease } from './release.mjs';

// The console comes from the archive's name (OpenBLOPS-NX.zip or OpenBLOPS-NX2.zip).
const target = Object.values(consoles).find(({ archive }) => archive === basename(process.argv[2])) ?? consoles.switch;
const archive = await readFile(process.argv[2]);
const programs = await readRelease(archive.buffer.slice(archive.byteOffset, archive.byteOffset + archive.byteLength), target);
for (const [name, bytes] of programs) console.log(`${name} (${bytes.length} bytes)`);
