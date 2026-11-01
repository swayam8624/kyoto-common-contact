import fs from 'node:fs/promises';
import {Presentation,PresentationFile} from '@oai/artifact-tool';
const root='/Users/swayamsingal/Desktop/Programming/kyoto-common-contact';
const d=root+'/.presentation-review-2026-11-02',ev=root+'/output/review-2026-11-02';
const results=JSON.parse(await fs.readFile(d+'/results.json','utf8'));
const p=Presentation.create({slideSize:{width:1280,height:720}});
const C={ink:'#172536',muted:'#566575',blue:'#2667B8',teal:'#167C75',amber:'#986D31',line:'#D9E0E7',pale:'#F1F5F7'};
function text(s,str,x,y,w,h,size=28,color=C.ink,bold=false){const q=s.shapes.add({geometry:'textbox',position:{left:x,top:y,width:w,height:h},fill:'none',line:{fill:'none',width:0}});q.text=str;q.text.style={typeface:'Arial',fontSize:size,color,bold};return q;}
function rect(s,x,y,w,h,fill){return s.shapes.add({geometry:'rect',position:{left:x,top:y,width:w,height:h},fill,line:{fill:'none',width:0}})}
function slide(title,tag='RESEARCH PROTOTYPE'){const s=p.slides.add();s.background.fill='#FFFFFF';text(s,tag,58,29,1110,28,16,C.teal,true);text(s,title,58,76,1164,94,40,C.ink,true);rect(s,58,654,1164,1,C.line);text(s,'Swayam Singal · Kyoto research discussion · 2 November 2026',58,673,1080,23,15,C.muted);text(s,String(p.slides.items.length).padStart(2,'0'),1172,673,50,23,15,C.muted);return s;}
async function image(s,file,x,y,w,h,alt){s.images.add({blob:new Uint8Array(await fs.readFile(file)),contentType:'image/png',alt,position:{left:x,top:y,width:w,height:h},fit:'contain'});}
function note(s,v){s.speakerNotes.text=v;}
function point(s,title,body,y,color=C.teal){text(s,title,62,y,310,44,27,color,true);text(s,body,390,y,815,91,28);}
function table(s,values,x,y,w,h,widths,size=24){const t=s.tables.add({rows:values.length,columns:values[0].length,left:x,top:y,width:w,height:h,columnWidths:widths,values});t.borders.assign({fill:C.line,width:1});for(let r=0;r<values.length;r++)for(let c=0;c<values[0].length;c++){const cell=t.getCell(r,c);cell.fill=r===0?C.ink:r%2===0?C.pale:'#FFFFFF';cell.text.style={typeface:'Arial',fontSize:size,color:r===0?'#FFFFFF':C.ink,bold:r===0};}return t;}
const src={ipc:'https://ipc-sim.github.io/file/IPC-paper-fullRes.pdf',ogc:'https://ankachan.github.io/Projects/OGC/',xpbd:'https://matthias-research.github.io/pages/publications/XPBD.pdf',ccfr:'https://arxiv.org/html/2607.26901v1',physx:'https://nvidia-omniverse.github.io/PhysX/physx/5.1.0/docs/RigidBodyCollision.html',ccd:'https://continuous-collision-detection.github.io/scalable_ccd/'};
let s=slide('Online adaptation for a common broad phase','CONTACT SIMULATION · IMPLEMENTATION AND PROPOSAL');
text(s,'Adjust collision-search work as cloth deforms,\nwhile retaining the caller’s required pairs.',62,193,1130,100,34,C.ink,true);
await image(s,ev+'/cases/one-sap-scene.png',64,320,650,290,'Actual native OpenGL one-sheet SAP scene; scene crop from recorded framebuffer');
text(s,'One geometric bounds format\nLocal candidate certificates\nMeasured contact-work feedback',785,337,425,159,29,C.teal,true);
text(s,'Working C++ prototype\nEvidence from one local cloth family',785,535,425,80,24,C.muted);
note(s,'Scope: three-dimensional native GLFW/OpenGL application, XPBD-style cloth test solver, shared broad-phase services and online controller. No official IPC/OGC/CCFR implementation is integrated. Cover scene is cropped from cases/one-sap.png; the full framebuffer and per-step CSV are retained. Current source and executable hashes: after/metadata.json.');

s=slide('The question is total collision cost');
point(s,'Pressure point','Deformation changes spatial overlap, invalidation frequency and precise-contact work.',190);
point(s,'Tradeoff','Larger guards save rebuilds but admit extra pairs. Smaller regions add bookkeeping.',330,C.blue);
point(s,'Research question','Can observed local work select useful partitions and update strategies during simulation?',470,C.amber);
note(s,'We optimize candidate infrastructure, with the narrow phase and solver held fixed. The objective must include maintenance, candidate filtering, precise contact work and controller cost. Candidate count alone is insufficient. CCFR §4.3 and Fig10 motivate collision-stage measurement; '+src.ccfr);

s=slide('Contact methods keep different responsibilities','LITERATURE');
table(s,[['Work','Core idea','What a common broad phase must preserve'],['IPC · 2020','Barrier contact with CCD-limited optimization','Required contact and motion queries'],['OGC · 2025','Offset geometry with conservative motion bounds','Offset-dependent neighborhood semantics'],['XPBD · 2016','Compliant position constraints','Caller’s collision and solver requirements'],['CCFR · 2026','Local convex regions for feasible displacement','Primitive neighborhoods for region construction']],60,184,1160,365,[190,455,515],23);
text(s,'Shared candidate infrastructure does not make the contact models interchangeable.',62,581,1145,57,27,C.teal,true);
note(s,'Primary sources: IPC '+src.ipc+' ; OGC '+src.ogc+' ; XPBD '+src.xpbd+' ; CCFR '+src.ccfr+'. IPC Algorithm1/§4.4, OGC abstract and method, XPBD abstract/constraint formulation, CCFR §3 and Appendix B. These are literature roles, not implemented external adapters.');

s=slide('Established ideas leave a narrower research question','PRIOR ART AND GAP');
point(s,'Already established','Spatial hierarchies, fat bounds, temporal reuse and automatic box pruning have prior art.',184);
point(s,'This prototype asks','Can confirmed contact work guide topology-aligned patch changes under a shared certificate?',322,C.blue);
point(s,'Still unproven','Novelty, strong-baseline superiority and transfer across solvers require further evidence.',462,C.amber);
text(s,'Relevant comparators: Bullet DBVT · PhysX ABP/PABP · conservative CCD broad phases',62,604,1150,35,20,C.muted);
note(s,'PhysX officially documents SAP/MBP/ABP/PABP; '+src.physx+'. Scalable CCD supplies conservative broad/narrow algorithms and benchmark data; '+src.ccd+'. Bullet is actually exercised in our isolated benchmark. This establishes relevant prior art, not an exhaustive novelty search. The open question is evaluation of this particular online policy, not whether adaptive broad phase exists.');

s=slide('One pipeline; the solver keeps ownership','IMPLEMENTATION');
const stages=[['Predict / constrain','XPBD-style cloth'],['Prepare bounds','IDs, masks, margin'],['Find candidates','Certified region cache'],['Precise response','VF / EE / sphere']];
for(let i=0;i<4;i++){const x=62+i*296;rect(s,x,215,260,4,i===2?C.teal:C.line);text(s,stages[i][0],x,243,262,78,29,i===2?C.teal:C.ink,true);text(s,stages[i][1],x,340,260,79,24,C.muted);if(i<3)text(s,'→',x+265,285,36,52,30,C.blue,true);}
text(s,'Repeat within each timestep. Feed measured work to the online controller once per step.',62,478,1130,87,30,C.ink,true);
text(s,'C++ state → service candidates → contact corrections → same-process native rendering',62,596,1140,35,23,C.teal);
note(s,'MultiCloth::step in src/research_multicloth.cpp invokes five collision queries and calls OnlinePartitioner::observe at full-frame boundaries. Actual candidate IDs feed projectContacts. Native renderer consumes the same positions vector. Python validates and plots measurements after C++ runs; it does not drive this solver. VF=vertex-face, EE=edge-edge.');

s=slide('A reuse certificate constrains every query','CORRECTNESS CONTRACT');
await image(s,d+'/certificate.png',58,190,1165,155,'For all i, current padded bound B_i(t) is contained in envelope E_i; current overlap pairs are a subset of cached pairs');
point(s,'Enclose','Raw min/max AABBs, stable IDs, category/mask and movable flags; explicit margin.',365);
point(s,'Invalidate','An escaped bound, changed mask, request or partition requires rebuilding before use.',477,C.blue);
text(s,'Regional blocks are rechecked against current bounds. This certifies discrete queries, not CCD.',62,603,1155,35,23,C.amber,true);
note(s,'B_i is a current half-margin-padded primitive AABB; E_i is its stored guard. Inclusion plus unchanged eligibility makes current overlaps a subset of cached envelope overlaps. RegionalCandidates rechecks cached IDs against tight current boxes, sorts canonical pairs, and returns the current candidate set. Full partition changes reset certificates. Generic bounds are not a swept-motion enclosure. Proof and tests: docs/ONLINE_ALGORITHM.md, CODE_AUDIT_CORE.md.');

s=slide('Online feedback changes patches and traversal','IMPLEMENTED POLICY');
point(s,'Observe','AABB tests, cached-pair rechecks, invalidations and executed contact corrections.',181);
point(s,'Smooth and decide','EWMA signals; six-frame decisions with cooldowns and bounded structural edits.',312,C.blue);
point(s,'Apply safely','Split hot patches; merge quiet siblings; select flat or hierarchical search; invalidate caches.',445,C.amber);
text(s,'Patch spans: 12 → 6 → 3 mesh cells. Particles and physical constraints stay fixed.',62,600,1150,37,24,C.teal,true);
note(s,'src/online_partitioner.cpp: W = AABB tests + 0.65 cached checks + 2 rejected candidates + 3 invalidations. EWMA beta=0.22. Heuristic thresholds; two hot windows allow split, three quiet windows for siblings allow merge, 12-frame structural cooldown, 18-frame traversal cooldown. This is runtime control, not an offline calibration table. Contact signal counts successful corrections, not unique physical contact points.');

s=slide('Every comparison has an exercised code path','ALGORITHM INVOCATION AUDIT');
table(s,[['Path','Actually exercised by','Evidence'],['SAP / global guarded','Coupled solver + native policy selector','Three repeats; paired states'],['Regional flat / two-tier','Same solver; fixed and adaptive budgets','All seven policy rows'],['Online split / merge / switch','Per-step controller + regional query','Live trace; merge regression'],['Bullet DBVT / dynamic DBVT','Isolated AABB benchmark, not cloth solver','Six workloads × nine methods'],['Self-contact / grabbing','Shared precise response / barycentric attachment','Headless fold and grab regressions']],60,180,1160,385,[330,480,350],22);
text(s,'Baselines are deliberate ablations; benchmark results do not integrate Bullet into cloth physics.',62,610,1150,32,22,C.muted);
note(s,'Invocation matrices: docs/CODE_AUDIT_CORE.md and docs/EVIDENCE_METHODS_AUDIT.md. All 54 isolated output rows executed; all 21 coupled policy/sheet combinations run per repetition. Online merge is exercised in changing-workload regression, not observed in the 160-frame physics trace. Legacy rigid-sphere islands/analytic TOI are separate testbed paths, not cloth CCD.');

s=slide('Native case: three interacting sheets','ACTUAL OPENGL FRAMEBUFFER · STEP 160');
await image(s,ev+'/cases/three-online.png',58,171,850,468,'Native window showing three cloths and moving spheres, online policy, full visible HUD');
text(s,'Online region control',942,195,280,79,29,C.teal,true);
text(s,'Same C++ solver\nSelf-contact enabled\nTwo moving spheres',942,300,280,143,26);
text(s,'Full frame, CSV and\nconfiguration retained',942,514,280,83,23,C.muted);
note(s,'Source: cases/three-online.png, .csv and .log. CLI advances exactly 160 actual fixed steps then pauses. No replay mesh or Blender simulation is loaded. Intersheet/sphere corrections are observed; zero self corrections at this frame does not demonstrate a fold. Timings in a screenshot are one frame, not the repeated benchmark result.');

s=slide('A matched native case checks the comparison','SAP REBUILD · SAME STEP 160');
await image(s,ev+'/cases/three-sap.png',58,172,850,465,'Native OpenGL three-sheet SAP reference at step160, matching the online simulation state');
text(s,'Same state, different\ncandidate maintenance',941,203,280,132,28,C.teal,true);
text(s,'All 160 recorded state\nchecksums match the\nonline case.',941,382,280,120,25);
text(s,'Full coordinate comparison\nis also run headlessly.',941,543,280,77,22,C.muted);
note(s,'Source cases/three-sap.png/.csv/.log and cases/three-online.csv. All160 paired native trace state hashes match; the benchmark separately checks every particle coordinate. Native mouse automation returned noWindowsAvailable or no positive grab rows, so a live drag is NOT presented as verified evidence. Picking/attachment/self-contact routines pass targeted headless regressions. See VERIFICATION.md for this practical limit.');

s=slide('Measure coverage, physics and cost separately','EXPERIMENT DESIGN');
point(s,'Controlled comparison','1/2/3 sheets × seven policies; identical forces, constraints and timestep. Three local repeats.',181);
point(s,'Independent checks','Compare every particle against SAP; periodically audit candidates with exhaustive AABB search.',313,C.blue);
point(s,'Timing boundary','Complete CPU step includes periodic oracle checks; rendering is excluded. Isolated AABB timing is separate.',445,C.amber);
text(s,'Δt = 1/90 s · 5 solver iterations · 60 steps/policy (warm-up: 8)\nOnline trace: 160 steps (warm-up: 12). Same settings across policies.',62,580,1155,62,23,C.muted);
note(s,'Raw records after/multicloth-{1,2,3}.csv (63 aggregate rows) and after/online-{1,2,3}.csv (480 per-frame rows). Apple M2 Pro, AppleClang Release; metadata records source/executable hashes. Repeats are on one deterministic scene family, not independent scene samples. Method execution order is fixed in the seven-way comparison; timing-guided controller decisions can vary. Confidence intervals are not claimed.');

s=slide('Online improves on SAP here; global guard wins','LOCAL RESULTS · THREE CLOTHS');
await image(s,d+'/policies.png',58,166,835,470,'Seven-policy full CPU step comparison, three local repeat means with observed trial-mean ranges');
const q=results['3'];text(s,`${q.sap_rebuild.mean.toFixed(2)} ms`,934,207,282,58,35,C.muted,true);text(s,'SAP rebuild',934,264,282,40,24,C.muted);text(s,`${q.online_contact_work.mean.toFixed(2)} ms`,934,337,282,58,35,C.teal,true);text(s,'Online controller',934,394,282,40,24,C.teal);text(s,`${q.global_guarded.mean.toFixed(2)} ms`,934,468,282,58,35,C.amber,true);text(s,'Global guard',934,525,282,40,24,C.amber);
text(s,'Bars: mean of three trial means. Whiskers: min–max trial means, not confidence intervals.',62,615,1150,29,20,C.muted);
note(s,'Computed directly from fresh after/multicloth-{1,2,3}.csv, sheets=3, 52 timed steps per variant/trial. Full CPU timings include periodic audit costs. All 63 rows have maxPositionError=0. Online is not the fastest tested policy; small-fixture overhead can outweigh local pruning. Do not attribute this chart to an IPC/OGC adapter, GPU kernel or general speedup.');

s=slide('The controller changes real runtime structure','ONLINE TRACE · FIRST OF THREE REPEATS');
await image(s,d+'/online-trace.png',58,165,856,474,'Online versus SAP step timing and actual region leaf count over 160 physical steps');
const tr=results.trace[0];text(s,`${tr.splits} splits\n${tr.switches} strategy switches`,948,215,270,139,32,C.teal,true);text(s,`${tr.merges} observed merges`,948,396,270,52,27,C.amber,true);text(s,'Merging is tested in a\nseparate quiet-workload\nregression.',948,474,270,111,24,C.muted);
note(s,'after/online-1.csv. Blue vertical markers indicate actual split frames; region count grows 12 to 48. This repeat performs 12 splits, zero merges, four strategy switches. All three repeats compare every frame with SAP with maximum error0 and audit11 frames each. The time spikes include periodic oracle work. Selection windows do not provide same-frame A/B causal proof.');

s=slide('Audit fixes make the evidence harder to misread','CODE SAFETY AND REDUNDANCY');
table(s,[['Found','Changed','Verification'],['Finite inputs could overflow padding','Validate padded bounds and guards before cache use','Overflow + recovery regression'],['Redundant pair pass / dead helpers','Remove proven redundant unique pass and unused code','Multi-region uniqueness + equivalence'],['tee could hide failed benchmark','Bash pipefail; reject incomplete/invalid CSVs','Malformed-data and shell fixtures'],['Native trace / HUD ambiguity','Per-step scene, policy, grab and audit flags; fit HUD','Window captures and CLI checks']],60,180,1160,363,[365,440,355],22);
text(s,'Seven Release test suites pass. The 480-step stability probe remains bounded.',62,582,1150,51,27,C.teal,true);
note(s,'Exact changes and commands: CODE_AUDIT_CORE.md, EVIDENCE_METHODS_AUDIT.md, NATIVE_CASES.md and output/review-2026-11-02/VERIFICATION.md. Removal of one redundant pass is an optimization; no statistically established code-audit speedup is claimed. Sanitizer results are recorded separately. Native finite-state checks pause runaway motion.');

s=slide('Coverage is necessary; it is not physical validation','LIMITATIONS');
point(s,'Contact model','Approximate discrete response; fast folds can tunnel. Self-contact is not a nonpenetration proof.',181);
point(s,'Stabilization','480-step max edge stretch 1.15×; damping and 12,105 velocity clamps affect the trajectory.',316,C.amber);
point(s,'Generalization','Small fixed-resolution cloth family, CPU implementation, heuristic controller, no external solver adapter.',454,C.blue);
text(s,'Equal trajectories show broad-phase agreement in these runs; they do not certify material accuracy.',62,601,1150,37,23,C.muted);
note(s,'after/stability.log: max absolute coordinate2.45379, max frame displacement.08324, structural edge stretch1.14950, floor minZ.04000, velocityClamps12105 over480frames. These are implementation safeguards and engineering bounds, not physical calibration. No general historical originality or solver-independent performance claim.');

s=slide('Next experiments should challenge the policy','FUTURE WORK');
point(s,'1 · Stronger evaluation','Larger independent scenes; randomized order; stage costs, tail latency and controller ablations.',182);
point(s,'2 · One real adapter','Keep an existing method’s narrow phase and solver; validate its exact margin and motion contract.',326,C.blue);
point(s,'3 · Stronger safety','Explicit swept-bound requests and robust CCD; test pathological folds and floating-point edge cases.',471,C.amber);
note(s,'Prioritize comparison to persistent dynamic BVHs and automatic pruning on compatible workloads, not only rebuild baselines. A true external adapter must preserve method-specific motion/offset requirements. Scalable CCD provides relevant conservative algorithms and benchmark data: '+src.ccd);

s=slide('Sources and reproducible evidence','REFERENCES');
text(s,'IPC — Li et al., 2020 · Algorithm 1, Table 1, Fig. 23\nOGC — Chen et al., 2025 · Table 1, Figs. 17–18\nXPBD — Macklin et al., 2016 · compliant constraint formulation\nCCFR — Kikuchi & Kanai, 2026 · §4.3, Table 1, Figs. 10–11',62,185,1150,199,26);
text(s,'Metric choices: full runtime and collision-stage cost; contact workload;\nmethod-specific convergence and remaining violations.',62,419,1130,85,28,C.teal,true);
text(s,'Local bundle: output/review-2026-11-02/\nRaw CSVs · full native captures · logs · source/build hashes · verification report',62,548,1140,72,24,C.muted);
note(s,Object.entries(src).map(([k,v])=>k+': '+v).join('\n')+'\nEvaluation anchors refer to quantities used in the papers, not numerical comparisons against them. OGC publication/project page checked; source paper location retained from earlier audit. All demo figures come from actual local C++ measurements or native framebuffers. No paper result image is substituted for implemented output.');

await fs.mkdir(d+'/renders',{recursive:true});
for(let i=0;i<p.slides.items.length;i++){const b=await p.export({slide:p.slides.items[i],format:'png',scale:1});await fs.writeFile(d+`/renders/slide-${String(i+1).padStart(2,'0')}.png`,new Uint8Array(await b.arrayBuffer()));}
await (await PresentationFile.exportPptx(p)).save(d+'/candidate.pptx');console.log('Rendered',p.slides.items.length,'research slides');
