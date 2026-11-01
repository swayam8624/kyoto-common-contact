import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import vm from 'node:vm';
import sharp from 'sharp';

const root = new URL('../', import.meta.url);
const htmlPath = new URL('output/Common_Broad_Phase_Demo.html', root);
const outputDir = new URL('./', import.meta.url);
const html = await fs.readFile(htmlPath, 'utf8');
const inlineScript = html.match(/<script>([\s\S]*?)<\/script>/)?.[1];
assert.ok(inlineScript, 'standalone inline script exists');

class Element {
  constructor(tag = 'div', attrs = {}) {
    this.tag = tag;
    this.attrs = { ...attrs };
    this.children = [];
    this.textContent = '';
    this.innerHTML = '';
    this.value = '';
    this.dataset = {};
    this.className = '';
    this.onclick = null;
    this.oninput = null;
    this.classList = { toggle: (name, enabled) => { this.hidden = enabled; this.hiddenClass = name; } };
  }
  setAttribute(key, value) { this.attrs[key] = String(value); }
  getAttribute(key) { return this.attrs[key]; }
  appendChild(child) { this.children.push(child); return child; }
  replaceChildren(...children) { this.children = children; }
}

const ids = ['scene', 'time', 'distance', 'gap', 'candidateState', 'timeText', 'candidateNote',
  'preciseNote', 'precise', 'play', 'summary', 'stageTitle', 'stageCopy'];
const elements = new Map(ids.map(id => [id, new Element(id === 'scene' ? 'svg' : 'div')]));
elements.get('scene').attrs = { viewBox: '0 0 800 410' };
const stageButtons = ['geometry', 'bounds', 'candidate'].map(stage => {
  const button = new Element('button', { 'aria-pressed': 'false' });
  button.dataset.stage = stage;
  return button;
});
const document = {
  getElementById(id) { assert.ok(elements.has(id), `known element #${id}`); return elements.get(id); },
  querySelectorAll(selector) { assert.equal(selector, '[data-stage]'); return stageButtons; },
  createElementNS(_ns, tag) { return new Element(tag); },
};
const context = { document, console, setInterval, clearInterval };
vm.runInNewContext(inlineScript, context, { filename: 'Common_Broad_Phase_Demo.inline.js' });

const scene = elements.get('scene');
const candidateState = elements.get('candidateState');
assert.equal(candidateState.textContent, 'RETAINED');
assert.equal(elements.get('distance').textContent, '0.3437');
assert.match(elements.get('candidateNote').textContent, /False positive/);
assert.equal(scene.children.find(node => node.tag === 'polygon').tag, 'polygon');
assert.ok(scene.children.some(node => node.tag === 'rect' && node.attrs.stroke === '#d78300'), 'false-positive bounds are amber');
const polygonPoints = scene.children.find(node => node.tag === 'polygon').attrs.points.split(' ').map(pair => pair.split(',').map(Number));
assert.ok(polygonPoints.every(([x, y]) => x >= 0 && x <= 800 && y >= 0 && y <= 410), 'all triangle vertices fit inside the SVG viewBox');
assert.equal(stageButtons.find(button => button.dataset.stage === 'candidate').getAttribute('aria-pressed'), 'true');

const range = elements.get('time');
range.value = '40';
range.oninput();
assert.equal(elements.get('distance').textContent, '0.0800');
assert.match(elements.get('candidateState').className, /yes/);
assert.equal(elements.get('candidateNote').textContent, 'The padded AABBs overlap, and exact distance is within δ.');

stageButtons.find(button => button.dataset.stage === 'bounds').onclick();
assert.equal(elements.get('stageTitle').textContent, 'Bounds');
assert.equal(stageButtons.find(button => button.dataset.stage === 'bounds').getAttribute('aria-pressed'), 'true');
assert.equal(scene.children.filter(node => node.tag === 'rect' && node.attrs.stroke !== 'none').length, 2, 'bounds stage draws two outlined bounds');
stageButtons.find(button => button.dataset.stage === 'candidate').onclick();
range.value = '64';
range.oninput();
elements.get('precise').onclick({ currentTarget: elements.get('precise') });
assert.equal(elements.get('precise').textContent, 'Hide precise check');
assert.match(elements.get('preciseNote').textContent, /0\.3437 > δ/);

function escapeXml(text) {
  return String(text).replaceAll('&', '&amp;').replaceAll('<', '&lt;').replaceAll('>', '&gt;').replaceAll('"', '&quot;');
}
function serialize(node) {
  const attrs = Object.entries(node.attrs).map(([key, value]) => ` ${key}="${escapeXml(value)}"`).join('');
  const body = `${escapeXml(node.textContent)}${node.children.map(serialize).join('')}`;
  return `<${node.tag}${attrs}>${body}</${node.tag}>`;
}
const svg = `<?xml version="1.0" encoding="UTF-8"?>${serialize(scene)}`;
const svgPath = new URL('offline-scene.svg', outputDir);
const pngPath = new URL('offline-scene.png', outputDir);
await fs.writeFile(svgPath, svg);
await sharp(Buffer.from(svg)).png().toFile(pngPath.pathname);
const metadata = await sharp(pngPath.pathname).metadata();
assert.equal(metadata.width, 800);
assert.equal(metadata.height, 410);

console.log('Standalone UI script: initial false positive, near-contact timeline, stage buttons, and precise toggle passed.');
console.log(`SVG/PNG scene saved: ${svgPath.pathname} (${metadata.width}x${metadata.height}).`);
