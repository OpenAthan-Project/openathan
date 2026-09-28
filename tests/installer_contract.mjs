// Import the real pinned website validator; only temporary synthetic bytes enter it.
import { readFile } from 'node:fs/promises';
import { pathToFileURL } from 'node:url';
import assert from 'node:assert/strict';
const [modulePath, bundle] = process.argv.slice(2);
const { checkedManifest, parsePin, verifyBundle, sha256 } = await import(pathToFileURL(modulePath));
const bytes = new Uint8Array(await readFile(`${bundle}/manifest.json`));
const pin = parsePin({ schema: 1, release: {
  tag: 'v0.1.0', manifestSha256: await sha256(bytes), mediaReviewed: true, hardwareQualified: true,
}});
const manifest = await checkedManifest(bytes, pin);
const inputs = new Map();
for (const part of manifest.parts) inputs.set(part.file, new Uint8Array(await readFile(`${bundle}/${part.file}`)));
assert.equal((await verifyBundle(manifest, inputs)).parts.length, 2);
inputs.get('athan-audio.bin')[4096] ^= 1;
await assert.rejects(verifyBundle(manifest, inputs));
console.log('Pinned installer accepts producer output and rejects changed audio. Synthetic test only.');
