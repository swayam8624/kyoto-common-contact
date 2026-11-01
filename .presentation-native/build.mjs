import fs from 'node:fs/promises';
import { Presentation, PresentationFile } from '@oai/artifact-tool';

const root = '/Users/swayamsingal/Desktop/Programming/kyoto-common-contact';
const build = root + '/.presentation-native';
const out = root + '/output';
const P = Presentation.create({ slideSize: { width: 1280, height: 720 } });
const C = { bg: '#F7F8FA', white: '#FFFFFF', ink: '#172536', muted: '#5C6A78', line: '#D7DDE4', blue: '#2468B4', teal: '#18877F', amber: '#B87916', paleBlue: '#EAF1F8', paleTeal: '#E7F3F1', paleAmber: '#F7F0E3', red: '#A8493D' };

function shape(s, geom, x, y, w, h, fill='none', stroke='none', sw=0) {
  return s.shapes.add({ geometry: geom, position: { left: x, top: y, width: w, height: h }, fill, line: { fill: stroke, width: sw } });
}
function txt(s, str, x, y, w, h, size=28, color=C.ink, bold=false, font='Arial') {
  const q = shape(s, 'textbox', x, y, w, h);
  q.text = str;
  q.text.style = { typeface: font, fontSize: size, color, bold, verticalAlignment: 'middle' };
  return q;
}
function rule(s, x, y, w, color=C.line, h=2) { shape(s, 'rect', x, y, w, h, color); }
function base(title, section='COMMON CONTACT PIPELINE') {
  const s = P.slides.add(); s.background.fill = C.bg;
  txt(s, section, 58, 28, 900, 26, 16, C.blue, true);
  txt(s, title, 58, 68, 1160, 62, 39, C.ink, true);
  rule(s, 58, 642, 1164, C.line, 1);
  txt(s, 'Kyoto research discussion · 2 Nov 2026', 58, 652, 980, 26, 15, C.muted);
  txt(s, String(P.slides.items.length).padStart(2, '0'), 1160, 652, 58, 26, 15, C.muted, true);
  return s;
}
async function img(s, file, pos, alt) {
  const bytes = new Uint8Array(await fs.readFile(file));
  s.images.add({ blob: bytes, contentType: 'image/png', alt, position: pos, fit: 'contain' });
}
function notes(s, content) { s.speakerNotes.text = content; }
function bullet(s, label, body, y, accent=C.blue) {
  shape(s, 'rect', 66, y+10, 6, 48, accent);
  txt(s, label, 88, y, 300, 34, 22, accent, true);
  txt(s, body, 88, y+36, 1070, 58, 27, C.ink);
}

// 1. Opening claim and status
{
  const s = P.slides.add(); s.background.fill = C.bg;
  txt(s, 'COMMON CONTACT PIPELINE', 66, 46, 900, 30, 17, C.blue, true);
  txt(s, 'A reusable broad phase\ninside a live C++ physics demo', 66, 118, 880, 170, 48, C.ink, true);
  txt(s, 'The first result is narrow but measurable: guarded candidate reuse reduced CPU step time in one deterministic XPBD cloth experiment.', 68, 330, 725, 105, 29, C.muted);
  shape(s, 'rect', 865, 152, 342, 258, C.paleTeal);
  txt(s, '0.999 ms', 892, 188, 288, 72, 48, C.teal, true);
  txt(s, 'mean CPU step\nguarded reuse', 895, 260, 282, 72, 25, C.ink);
  rule(s, 895, 344, 270, '#B8DAD5', 2);
  txt(s, 'vs 2.351 ms\nrebuild each iteration', 895, 354, 282, 60, 23, C.ink);
  txt(s, 'Exploratory result · Apple M2 Pro · fixed scene and settings · no IPC / OGC adapter', 68, 542, 1100, 42, 20, C.muted);
  txt(s, 'Kyoto research discussion · Swayam Singal · 2 November 2026', 68, 658, 900, 28, 16, C.muted);
  notes(s, 'This is a working research prototype and a bounded measurement, not a universal speedup claim. The comparison uses the same C++ XPBD cloth solver, SAP margin 0.05, displacement budget 0.04, fixed dt 1/120, and paired runs on an Apple M2 Pro. See benchmark slide and output/native/benchmark-summary.txt.');
}

// 2. Research question and boundary
{
  const s = base('Can candidate generation be shared safely?');
  txt(s, 'Many simulators repeatedly search for geometrically nearby primitives.', 68, 170, 1090, 50, 31, C.ink, true);
  txt(s, 'A common service could own conservative bounds, spatial search and stable pair output.', 68, 248, 1110, 62, 28, C.muted);
  rule(s, 68, 354, 1100, C.line, 2);
  txt(s, 'SHARED SERVICE', 68, 390, 370, 34, 21, C.blue, true);
  txt(s, 'Bounds · eligibility · candidate pairs · reuse policy', 68, 434, 485, 100, 28, C.ink);
  txt(s, 'METHOD OWNER', 670, 390, 420, 34, 21, C.teal, true);
  txt(s, 'Narrow phase · contact meaning · solver · tolerances', 670, 434, 500, 100, 28, C.ink);
  txt(s, 'The proposal shares search infrastructure, not contact mathematics.', 68, 565, 1110, 42, 26, C.ink, true);
  notes(s, 'Research question: can a reusable geometric candidate-generation interface support different simulators without weakening method-specific correctness requirements? IPC, OGC, XPBD and CCFR retain distinct contact semantics and solver assumptions. CCFR is a collision-handling method compatible with XPBD, not an independent XPBD solver. Sources are provided in speaker notes on the method boundary slide.');
}

// 3. Executable stage diagram
{
  const s = base('The demo keeps the stages visible');
  const blocks = [
    ['1', 'XPBD cloth state', 'C++ positions\nand velocities', C.paleBlue, C.blue],
    ['2', 'Shared broad phase', 'Bounds + guarded\nSAP candidates', C.paleTeal, C.teal],
    ['3', 'Precise query', 'Sphere–triangle\nproximity', C.paleAmber, C.amber],
    ['4', 'Contact response', 'XPBD cloth\nsolver update', C.white, C.ink]
  ];
  for (let i=0;i<blocks.length;i++) {
    const x=66+i*290; const [n,title,body,fill,accent]=blocks[i];
    shape(s,'rect',x,208,250,218,fill);
    txt(s,n, x+18,224,40,35,21,accent,true);
    txt(s,title,x+18,267,218,57,25,accent,true);
    txt(s,body,x+18,331,218,75,23,C.ink);
    if(i<blocks.length-1) txt(s,'›',x+254,284,35,50,32,C.muted,true);
  }
  txt(s, 'Observed in one process: the C++ solver consumes the service’s candidate output.', 68, 493, 1110, 44, 27, C.ink, true);
  txt(s, 'Discrete sphere–triangle query · no self-collision · no CCD', 68, 558, 1100, 36, 21, C.muted);
  notes(s, 'This is a full same-process demonstration: an XPBD cloth solver calls the generic BroadPhaseService and uses returned candidate pairs for precise sphere-triangle proximity. The service provides stable candidates. This prototype has no continuous collision detection, cloth self-collision, IPC integration or OGC integration. The GUI reports frame, candidate, proximity, CPU step and broad-phase timing.');
}

// 4. Input/output contract
{
  const s = base('A small contract carries the caller’s query meaning');
  txt(s, 'BOUNDS', 68, 170, 500, 34, 21, C.blue, true);
  txt(s, 'Primitive identity\n3D minimum and maximum\nCategory and collision mask\nMovable / fixed status', 68, 216, 495, 238, 28, C.ink);
  txt(s, 'REQUEST', 664, 170, 500, 34, 21, C.teal, true);
  txt(s, 'Query margin\nDisplacement budget\nUpdate policy\nSelected backend', 664, 216, 495, 238, 28, C.ink);
  rule(s, 68, 475, 1090, C.line, 2);
  txt(s, 'OUTPUT', 68, 508, 170, 35, 21, C.amber, true);
  txt(s, 'A stable, deduplicated list of candidate ID pairs', 252, 498, 910, 54, 30, C.ink, true);
  txt(s, 'Precise tests decide contact; the broad phase does not.', 68, 577, 1090, 36, 24, C.muted);
  notes(s, 'Implemented record: Bounds{id,min,max,category,mask,movable}; QueryRequest{margin,displacementBudget,updatePolicy}. The service expands each bound by half the requested margin. Candidate output is an actual pair list, not only a count. Downstream narrow phase decides precise contact. Caller controls geometry and semantics; current demo supports sphere-triangle proximity.');
}

// 5. Guarded reuse protocol
{
  const s = base('Reuse is bounded by a displacement envelope');
  const y=208, w=330, h=166;
  const items=[
    ['1  BUILD', 'Create padded bounds\nand cache SAP pairs.', C.paleBlue, C.blue],
    ['2  MOVE', 'Advance the same\nsolver and geometry.', C.paleTeal, C.teal],
    ['3  CHECK', 'Reuse only while each\ncurrent bound fits its guard.', C.paleAmber, C.amber]
  ];
  for(let i=0;i<items.length;i++){
    const x=68+i*380; const [head,body,fill,accent]=items[i];
    shape(s,'rect',x,y,w,h,fill); txt(s,head,x+20,y+18,w-40,36,22,accent,true); txt(s,body,x+20,y+66,w-40,84,25,C.ink);
    if(i<2) txt(s,'›',x+w+18,y+55,35,52,35,C.muted,true);
  }
  txt(s, 'Otherwise rebuild. Reuse also requires unchanged IDs, masks, request and backend.', 68, 430, 1090, 68, 27, C.ink, true);
  await img(s, build+'/certificate.png', {left:72,top:494,width:1125,height:82}, 'Typeset set inclusion certificate: current padded box overlaps are a subset of cached guarded-envelope overlaps');
  txt(s, 'Bᵢ(t): padded current bound · Eᵢ: stored envelope · unchanged IDs, masks, request and backend', 72, 578, 1120, 32, 19, C.muted);
  notes(s, 'Certificate: if each current padded bound B_i(t) is contained by its stored envelope E_i, then any pair of current padded bounds that overlap is also a pair of overlapping envelopes. The cached pair list is built from envelope overlaps. Reuse requires unchanged primitive IDs, eligibility masks, query request (including margin/budget/policy) and broad-phase backend. The certificate concerns discrete query bounds at evaluation time; it does not certify the continuous path between steps. In the measured run, margin was 0.05 and displacement budget 0.04.');
}

// 6. Initial scene evidence
{
  const s = base('Native demo: live C++ cloth–sphere scene');
  await img(s, build+'/native-initial.png', {left:62,top:152,width:920,height:468}, 'OpenGL framebuffer capture of native live C++ XPBD cloth and sphere scene at frame zero');
  txt(s, 'Same-process C++ XPBD solver', 1000, 194, 220, 90, 26, C.blue, true);
  txt(s, 'Live candidate and proximity counts', 1000, 302, 220, 100, 24, C.ink);
  txt(s, 'Pause, step, reset and inspect controls', 1000, 425, 220, 108, 24, C.ink);
  txt(s, 'Initial state · no file playback', 1000, 558, 220, 43, 18, C.muted);
  notes(s, 'Native OpenGL framebuffer capture from output/native/native-initial.png on Apple M2 Pro, OpenGL 4.1 Metal. Frame zero shows the actual cloth mesh and sphere before stepping. This is not a Blender rendering, offline playback, or a scripted animation. Controls shown in the running application: Q guarded reuse, M margin, B broad phase, F frame scene, R reset, X visual cutaway (disabled by default), W wire mode / drag orbit / zoom.');
}

// 7. Matched state comparison
{
  const s = base('Two broad-phase policies reach the same captured state');
  await img(s, build+'/native-baseline.png', {left:60,top:154,width:560,height:345}, 'Native C++ scene at frame 120 using SAP with guarded reuse disabled');
  await img(s, build+'/native-adaptive.png', {left:660,top:154,width:560,height:345}, 'Native C++ scene at frame 120 using SAP with guarded envelope reuse enabled');
  txt(s, 'Rebuild every iteration', 62, 512, 548, 38, 24, C.amber, true);
  txt(s, 'Guarded envelope reuse', 662, 512, 548, 38, 24, C.teal, true);
  txt(s, 'Frame 120 · same solver state hash · candidate set differs, trajectory does not', 62, 571, 1125, 40, 23, C.ink);
  notes(s, 'Both are native OpenGL framebuffer captures at frame 120, with cutaway off. The trace state hashes match exactly for this frame. Baseline log: output/native/baseline-capture.log. Adaptive log: output/native/adaptive-capture.log. The adaptive scene reports 937 candidates and 429 within-margin triangles; baseline reports 855 candidates and the same min gap. These visual candidate counts are at a single capture frame and are not the average timing workload. No oracle omissions were observed in the recorded capture checks.');
}

// 8. Bench results chart and quantified values
{
  const s = base('Measured CPU step time fell in this paired run');
  await img(s, build+'/benchmark.png', {left:60,top:148,width:750,height:385}, 'CPU step time chart showing mean p95 and maximum across rebuild and guarded reuse policies');
  txt(s, '2.351 ms', 850, 190, 320, 54, 33, C.amber, true);
  txt(s, 'mean · rebuild each iteration', 850, 240, 330, 35, 21, C.muted);
  txt(s, '0.999 ms', 850, 320, 320, 54, 33, C.teal, true);
  txt(s, 'mean · guarded reuse', 850, 370, 330, 38, 21, C.muted);
  rule(s, 850, 430, 325, C.line, 2);
  txt(s, 'Maximum', 850, 447, 320, 27, 18, C.muted, true);
  txt(s, '4.184 ms  →  2.951 ms', 850, 477, 350, 42, 22, C.ink, true);
  txt(s, '5 paired trials · 1,350 measured steps / policy · Apple M2 Pro · Release', 68, 548, 1110, 28, 19, C.ink, true);
  txt(s, 'CPU timing includes the oracle and excludes rendering · exploratory single-scene result', 68, 580, 1120, 28, 18, C.muted);
  notes(s, 'Source: output/native/benchmark-summary.txt and output/native/benchmark.png, generated from output/native/live_benchmark.csv. Mean/max/p95 (ms): rebuild 2.35117 / 4.18375 / 2.72708; guarded reuse 0.999491 / 2.9505 / 1.19762. The experiment ran Release C++ on Apple M2 Pro, same-process XPBD cloth, sweep-and-prune, margin 0.05, displacement budget 0.04, dt 1/120. Five paired trials of 300 steps per policy; first 30 steps of each trial warmed up, leaving 1,350 measured steps per policy. CPU step time is wall-clock CPU-side timing and excludes rendering. Results are exploratory for this implementation and machine; do not generalize to other geometries or simulators.');
}

// 9. Work saved and correctness checks
{
  const s = base('The time reduction tracks fewer broad-phase rebuilds');
  txt(s, '12,150', 68, 202, 320, 62, 44, C.amber, true);
  txt(s, 'generator calls\nrebuild each iteration', 68, 275, 325, 70, 24, C.ink);
  txt(s, '600', 476, 202, 320, 62, 44, C.teal, true);
  txt(s, 'generator calls\nguarded reuse', 476, 275, 325, 70, 24, C.ink);
  txt(s, '11,550', 884, 202, 320, 62, 44, C.blue, true);
  txt(s, 'cached pair-list reuses', 884, 275, 320, 70, 24, C.ink);
  rule(s, 68, 380, 1100, C.line, 2);
  bullet(s, 'Oracle coverage', '0 required-pair omissions in recorded comparisons', 414, C.teal);
  bullet(s, 'Solver agreement', 'Maximum position error: 0 across measured trajectories', 500, C.blue);
  txt(s, '2,700 measured CSV rows · 1,350 steps per policy · 5 trials per policy', 68, 594, 1110, 35, 21, C.muted);
  notes(s, 'Benchmark summary reports generatorCalls 12150 versus 600 and 11550 reuses, with maxPositionError 0 and measuredFramesPerPolicy 1350. The 2700-row CSV contains measured rows for both policies and excludes warmup. The native capture trace checks at frame 120 also have matching state hashes and zero oracle misses. These checks apply to this tested scene/configuration only and do not establish general equivalence for other methods.');
}

// 10. Interpreting against prior work
{
  const s = base('What to measure in a real method adapter');
  const rows = [
    ['Runtime', 'Build, query and full collision-stage time', 'IPC Table 1 · OGC Table 1 · CCFR Table 1', C.blue],
    ['Workload', 'Candidates, verified contacts and collision count', 'IPC Fig. 23 · OGC Fig. 18 · CCFR Table 1', C.teal],
    ['Convergence', 'Iterations and residuals where comparable', 'IPC Fig. 23 · OGC Fig. 17', C.amber],
    ['Safety', 'Post-step intersections under each method’s criteria', 'OGC §5 · CCFR Figs. 10–11', C.red],
  ];
  for(let i=0;i<rows.length;i++) {
    const y=169+i*92, [label,body,ref,accent]=rows[i];
    shape(s,'rect',68,y+8,6,62,accent);
    txt(s,label,91,y,215,31,22,accent,true);
    txt(s,body,91,y+34,625,45,24,C.ink);
    txt(s,ref,748,y+12,455,52,20,C.muted,true);
    rule(s,91,y+78,1118,C.line,1);
  }
  txt(s, 'Planned adapter measurements. This demo measures CPU step time and pair-cache work.', 68, 555, 1120, 48, 21, C.muted);
  notes(s, 'Literature measurement anchors retained from the proposal: IPC, Fig. 23 and Table 1 (https://ipc-sim.github.io/file/IPC-paper-fullRes.pdf); OGC, Figs. 17–18, Table 1 and §5 (https://graphics.cs.utah.edu/research/projects/ogc/Offset_Geometric_Contact-SIGGRAPH2025.pdf); CCFR, Table 1 and Figs. 10–11 (https://arxiv.org/html/2607.26901v1). These inform future evaluation design only. They are not measurements of this demo. CCFR is compatible with XPBD but has its own collision-free-region formulation.');
}

// 11. Research limits
{
  const s = base('The prototype boundary is explicit');
  txt(s, 'IMPLEMENTED', 68, 171, 430, 36, 21, C.teal, true);
  txt(s, 'C++23 core and live viewer\nXPBD cloth contact response\nSphere–triangle discrete query\nBrute-force oracle and sweep-and-prune\nGuarded reuse with cached pairs', 68, 222, 505, 260, 25, C.ink);
  txt(s, 'NOT IMPLEMENTED', 675, 171, 500, 36, 21, C.red, true);
  txt(s, 'Continuous collision detection\nCloth self-collision\nOfficial IPC / OGC integration\nCCFR implementation\nCross-method performance study', 675, 222, 505, 260, 25, C.ink);
  rule(s, 68, 520, 1090, C.line, 2);
  txt(s, 'This experiment tests an infrastructure seam, not solver interchangeability.', 68, 552, 1100, 49, 25, C.ink, true);
  notes(s, 'CCFR: Kikuchi and Kanai, “Convex Collision-Free Regions,” arXiv:2607.26901 (2026). It constructs local convex feasible regions for non-penetration and is compatible with XPBD, while remaining a distinct collision handling method. The prototype does not reproduce CCFR, IPC, or OGC and does not claim their guarantees. Official research project links: IPC https://ipc-sim.github.io/ ; OGC https://ankachan.github.io/Projects/OGC/ ; XPBD source https://github.com/InteractiveComputerGraphics/PositionBasedDynamics .');
}

// 12. Bounded next step / meeting close
{
  const s = base('The next test should change one method boundary');
  txt(s, '1', 68, 192, 68, 65, 45, C.blue, true); txt(s, 'Select one existing solver and one representative scene.', 146, 195, 1000, 57, 30, C.ink);
  txt(s, '2', 68, 296, 68, 65, 45, C.blue, true); txt(s, 'Replace only candidate generation with the shared service.', 146, 299, 1000, 57, 30, C.ink);
  txt(s, '3', 68, 400, 68, 65, 45, C.blue, true); txt(s, 'Compare pair coverage, collision outcomes and measured cost.', 146, 403, 1000, 57, 30, C.ink);
  rule(s, 68, 500, 1090, C.line, 2);
  txt(s, 'Which method and correctness requirement should anchor that adapter?', 68, 534, 1100, 62, 29, C.teal, true);
  notes(s, 'Discussion objective: agree on the first external adapter and its correctness oracle. Preserve the method’s own narrow phase, tolerances, solver and acceptance tests. The prototype demonstrates one seam in an XPBD cloth implementation; it does not yet prove reuse across heterogeneous simulators.');
}

await fs.mkdir(build+'/renders', {recursive:true});
for (let i=0; i<P.slides.items.length; i++) {
  const png = await P.export({slide:P.slides.items[i], format:'png', scale:1});
  await fs.writeFile(build+`/renders/slide-${String(i+1).padStart(2,'0')}.png`, new Uint8Array(await png.arrayBuffer()));
}
await (await PresentationFile.exportPptx(P)).save(build+'/candidate.pptx');
console.log(`Rendered ${P.slides.items.length} slides`);
