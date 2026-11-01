import fs from 'node:fs/promises';
import {Presentation,PresentationFile} from '@oai/artifact-tool';
const root='/Users/swayamsingal/Desktop/Programming/kyoto-common-contact';
const d=root+'/.presentation-refine-2026-11-02',assets=root+'/.presentation-review-2026-11-02',ev=root+'/output/review-2026-11-02';
const results=JSON.parse(await fs.readFile(assets+'/results.json','utf8'));
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
let s=slide('One broad phase, adapting to contact work','RESEARCH PROPOSAL · VERIFIED C++ PROTOTYPE');
text(s,'Shared candidate search.\nMethod-specific contact.',62,193,655,125,39,C.ink,true);
text(s,'The prototype changes collision-search regions\nusing measured work and confirmed corrections.',62,357,650,107,29,C.muted);
text(s,'One geometric format\nCertified candidate reuse\nOnline partition decisions',62,510,640,115,27,C.teal,true);
await image(s,ev+'/cases/one-sap-scene.png',760,173,460,421,'Actual native OpenGL cloth scene; crop from the one-sheet SAP framebuffer');
text(s,'Actual native C++ scene · one cloth case',760,604,460,30,18,C.muted);
note(s,'Scope: three-dimensional native GLFW/OpenGL application, XPBD-style cloth test solver, shared broad-phase services and online controller. No official IPC/OGC/CCFR implementation is integrated. Cover scene is cropped from cases/one-sap.png; the full framebuffer and per-step CSV are retained. Current source and executable hashes: after/metadata.json.');


s=slide('Less search does not always mean a faster step','PROBLEM AND OBJECTIVE');
text(s,'REUSE LARGER GUARDS',62,193,535,39,25,C.teal,true);
text(s,'Fewer candidate-list rebuilds.\nMore stale pairs may reach filtering\nand precise contact checks.',62,252,535,146,31);
text(s,'USE SMALLER REGIONS',690,193,527,39,25,C.blue,true);
text(s,'More local pruning.\nMore region bookkeeping, guard\nchecks and cache maintenance.',690,252,527,146,31);
rect(s,62,435,1150,2,C.line);
text(s,'Optimize complete collision cost',62,476,1150,48,33,C.ink,true);
text(s,'Measure search + candidate filtering + precise response + controller overhead.\nA smaller candidate set alone is not the objective.',62,548,1150,86,27,C.muted);
note(s,'We optimize candidate infrastructure, with the narrow phase and solver held fixed. The objective must include maintenance, candidate filtering, precise contact work and controller cost. Candidate count alone is insufficient. CCFR §4.3 and Fig10 motivate collision-stage measurement; '+src.ccfr);


s=slide('Contact methods keep different responsibilities','LITERATURE');
table(s,[['Work','Core idea','What a common broad phase must preserve'],['IPC · 2020','Barrier contact with CCD-limited optimization','Required contact and motion queries'],['OGC · 2025','Offset geometry with conservative motion bounds','Offset-dependent neighborhood semantics'],['XPBD · 2016','Compliant position constraints','Caller’s collision and solver requirements'],['CCFR · 2026','Local convex regions for feasible displacement','Primitive neighborhoods for region construction']],60,184,1160,365,[190,455,515],23);
text(s,'Shared candidate infrastructure does not make the contact models interchangeable.',62,581,1145,57,27,C.teal,true);
note(s,'Primary sources: IPC '+src.ipc+' ; OGC '+src.ogc+' ; XPBD '+src.xpbd+' ; CCFR '+src.ccfr+'. IPC Algorithm1/§4.4, OGC abstract and method, XPBD abstract/constraint formulation, CCFR §3 and Appendix B. These are literature roles, not implemented external adapters.');

s=slide('The research question is the control policy','ESTABLISHED METHODS → PROPOSED EVALUATION');
text(s,'ESTABLISHED MACHINERY',62,189,535,38,24,C.muted,true);
text(s,'Spatial hierarchies and sweeps\nFat bounds and temporal reuse\nAutomatic box pruning',62,250,535,166,31);
text(s,'OUR IMPLEMENTED POLICY',691,189,525,38,24,C.teal,true);
text(s,'Observe local work and corrections\nAdjust mesh-patch membership\nRebuild certificates before reuse',691,250,525,166,31);
rect(s,62,456,1150,2,C.line);
text(s,'Does contact-work feedback reduce total cost\nagainst strong baselines as the scene changes?',62,490,1130,96,34,C.ink,true);
text(s,'Policy novelty and transfer across solvers remain unproven. Comparators include Bullet DBVT and PhysX ABP/PABP.',62,608,1150,36,20,C.muted);
note(s,'PhysX officially documents SAP/MBP/ABP/PABP; '+src.physx+'. Scalable CCD supplies conservative broad/narrow algorithms and benchmark data; '+src.ccd+'. Bullet is actually exercised in our isolated benchmark. This establishes relevant prior art, not an exhaustive novelty search. The open question is evaluation of this particular online policy, not whether adaptive broad phase exists.');


s=slide('The shared stage sits inside the solver loop','PIPELINE AND OWNERSHIP');
const stages=[['Predict / constrain','Cloth positions and\ninternal constraints','CALLER'],['Prepare bounds','Raw min/max boxes\nand eligibility','CALLER'],['Find candidates','Certified regional\nsearch and reuse','SHARED SERVICE'],['Precise response','Vertex–face, edge–edge\nand sphere checks','CALLER']];
for(let i=0;i<4;i++){const x=62+i*296;text(s,stages[i][2],x,190,264,32,17,i===2?C.teal:C.muted,true);rect(s,x,239,260,4,i===2?C.teal:C.line);text(s,stages[i][0],x,271,261,82,29,i===2?C.teal:C.ink,true);text(s,stages[i][1],x,370,260,103,25,C.muted);if(i<3)text(s,'→',x+264,300,38,50,31,C.blue,true);}
rect(s,62,503,1150,2,C.line);
text(s,'Five collision queries per timestep; one controller update at the end.',62,535,1150,49,30,C.ink,true);
text(s,'Candidate IDs enter the same precise contact solver. The renderer reads the resulting C++ positions.',62,604,1150,38,23,C.teal);
note(s,'MultiCloth::step in src/research_multicloth.cpp invokes five collision queries and calls OnlinePartitioner::observe at full-frame boundaries. Actual candidate IDs feed projectContacts. Native renderer consumes the same positions vector. Python validates and plots measurements after C++ runs; it does not drive this solver. VF=vertex-face, EE=edge-edge.');


s=slide('What crosses the common interface?','SHARED GEOMETRIC FORMAT');
const cols=[62,458,854];
text(s,'GEOMETRY',cols[0],187,360,36,23,C.blue,true);
text(s,'Stable primitive ID\nXYZ min/max bounds\nCategory / mask\nMovable / fixed',cols[0],247,350,212,28);
text(s,'QUERY REQUIREMENTS',cols[1],187,360,36,23,C.teal,true);
text(s,'Proximity margin\nReuse / rebuild mode\nGuard budget\nDiscrete query scope',cols[1],247,350,212,28);
text(s,'CANDIDATE OUTPUT',cols[2],187,360,36,23,C.amber,true);
text(s,'Canonical ID pairs\nPossible interactions\nPrecise tests required',cols[2],247,350,170,28);
rect(s,62,488,1150,2,C.line);
text(s,'AABB = an axis-aligned min/max box around a primitive.',62,522,1150,42,27,C.ink,true);
text(s,'The caller supplies conservative geometry and contact semantics;\nthe shared service does not select forces or replace the solver.',62,582,1150,61,25,C.muted);
note(s,'Existing common record Bounds{id,min,max,category,mask,movable}. The global service uses QueryRequest{margin,displacementBudget,updatePolicy}; the regional path uses the same geometric record plus region/sheet metadata, margin and policy controls. This normalizes the conceptual boundary, not a claim that official IPC/OGC adapters already implement one universal API. Half the margin pads each primitive box. Current queries are discrete, not swept trajectories.');

s=slide('Reuse is justified by enclosure, not prediction','CERTIFICATE AND INVALIDATION');
await image(s,assets+'/certificate.png',58,150,1165,155,'Current padded bounds contained in cached envelopes imply current overlaps are included in cached pairs');
text(s,'B: current padded box   ·   E: cached guard   ·   C: overlap-pair set',62,300,1150,35,22,C.muted);
text(s,'STAYS INSIDE THE GUARD',62,347,535,36,23,C.teal,true);
text(s,'Reuse the cached pair superset.\nRecheck each pair against today’s\ntight padded boxes.',62,404,535,136,30);
text(s,'ESCAPES OR MEANING CHANGES',690,347,528,36,23,C.amber,true);
text(s,'Rebuild before answering.\nA changed mask, request or patch\nassignment also invalidates reuse.',690,404,528,136,30);
rect(s,62,568,1150,2,C.line);
text(s,'Why it works: an overlap inside smaller boxes must also exist in their enclosing guards.',62,590,1150,39,24,C.ink,true);
text(s,'This is a current-configuration coverage argument; it does not certify continuous motion.',62,630,1150,23,18,C.muted);
note(s,'B_i is a current half-margin-padded primitive AABB; E_i is its stored guard. Inclusion plus unchanged eligibility makes current overlaps a subset of cached envelope overlaps. RegionalCandidates rechecks cached IDs against tight current boxes, sorts canonical pairs, and returns the current candidate set. Full partition changes reset certificates. Generic bounds are not a swept-motion enclosure. Proof and tests: docs/ONLINE_ALGORITHM.md, CODE_AUDIT_CORE.md.');


s=slide('Measured work drives bounded online decisions','RUNTIME CONTROL LOOP');
const flow=[['Observe','Box tests, rechecks,\ninvalidations and corrections'],['Smooth','Reduce reaction to\nsingle-step noise'],['Decide','Six-frame windows\nplus cooldowns'],['Apply','Patch edits or traversal\nchange; reset certificates']];
for(let i=0;i<4;i++){const x=62+296*i;rect(s,x,194,259,3,i===3?C.teal:C.line);text(s,flow[i][0],x,219,260,50,31,C.teal,true);text(s,flow[i][1],x,291,260,110,24);if(i<3)text(s,'→',x+265,231,33,45,28,C.blue,true);}
text(s,'Split hot patches',62,439,535,44,29,C.blue,true);
text(s,'Localize expensive searches and rechecks.\nNew certificates cost work immediately.',62,498,535,98,26);
text(s,'Merge quiet siblings',690,439,525,44,29,C.amber,true);
text(s,'Reduce region bookkeeping when little\ncontact work is being observed.',690,498,525,98,26);
text(s,'The 12 → 6 → 3-cell hierarchy changes region IDs, not particles or physical constraints. Thresholds are heuristic.',62,615,1150,34,21,C.muted);
note(s,'src/online_partitioner.cpp: W = AABB tests + 0.65 cached checks + 2 rejected candidates + 3 invalidations. EWMA beta=0.22. Heuristic thresholds; two hot windows allow split, three quiet windows for siblings allow merge, 12-frame structural cooldown, 18-frame traversal cooldown. This is runtime control, not an offline calibration table. Contact signal counts successful corrections, not unique physical contact points.');


s=slide('Every comparison has an exercised code path','ALGORITHM INVOCATION AUDIT');
table(s,[['Path','Actually exercised by','Evidence'],['SAP / global guarded','Coupled solver + native policy selector','Three repeats; paired states'],['Regional flat / two-tier','Same solver; fixed and adaptive budgets','All seven policy rows'],['Online split / merge / switch','Per-step controller + regional query','Live trace; merge regression'],['Bullet DBVT / dynamic DBVT','Isolated AABB benchmark, not cloth solver','Six workloads × nine methods'],['Self-contact / grabbing','Shared precise response / barycentric attachment','Headless fold and grab regressions']],60,180,1160,385,[330,480,350],22);
text(s,'Baselines are deliberate ablations; benchmark results do not integrate Bullet into cloth physics.',62,610,1150,32,22,C.muted);
note(s,'Invocation matrices: docs/CODE_AUDIT_CORE.md and docs/EVIDENCE_METHODS_AUDIT.md. All 54 isolated output rows executed; all 21 coupled policy/sheet combinations run per repetition. Online merge is exercised in changing-workload regression, not observed in the 160-frame physics trace. Legacy rigid-sphere islands/analytic TOI are separate testbed paths, not cloth CCD.');

s=slide('Native evidence: the online policy is in the loop','THREE CLOTHS · TWO MOVING SPHERES · STEP 160');
await image(s,ev+'/cases/three-online.png',40,166,875,428,'Full actual native OpenGL framebuffer at step160 using the online policy');
text(s,'What it shows',944,184,279,86,29,C.teal,true);
text(s,'Live C++ geometry.\nCandidates feed\nprecise response.',944,300,279,136,25);
text(s,'Self-contact is ON.\nNo self-correction\nis shown this step.',944,476,279,116,23,C.muted);
text(s,'Full framebuffer, selected policy, configuration and per-step CSV are retained.',62,626,1150,24,18,C.muted);
note(s,'Source: cases/three-online.png, .csv and .log. CLI advances exactly 160 actual fixed steps then pauses. No replay mesh or Blender simulation is loaded. Intersheet/sphere corrections are observed; zero self corrections at this frame does not demonstrate a fold. Timings in a screenshot are one frame, not the repeated benchmark result.');


s=slide('Matched state; different candidate maintenance','SAP REBUILD REFERENCE · SAME STEP 160');
await image(s,ev+'/cases/three-sap.png',40,166,875,428,'Full native SAP reference framebuffer at the same step160');
text(s,'What is held fixed?',944,184,279,84,29,C.blue,true);
text(s,'Same forces.\nSame timestep.\nSame contact solver.',944,290,279,95,25);
text(s,'160 matching\nstate checksums',944,445,279,105,29,C.teal,true);
text(s,'The headless comparison also checks every particle coordinate; a screenshot alone cannot prove equivalence.',62,624,1150,26,19,C.muted);
note(s,'Source cases/three-sap.png/.csv/.log and cases/three-online.csv. All160 paired native trace state hashes match; the benchmark separately checks every particle coordinate. Native mouse automation returned noWindowsAvailable or no positive grab rows, so a live drag is NOT presented as verified evidence. Picking/attachment/self-contact routines pass targeted headless regressions. See VERIFICATION.md for this practical limit.');


s=slide('Read the HUD counters at their actual scope','METRIC INTERPRETATION');
const scope=[['Latest query','Candidates','Possible pairs returned by the last query. Extra pairs are allowed; they are not confirmed collisions.',C.blue],['Whole timestep','Contact corrections','Executed positional corrections across solver iterations. One pair can contribute more than once.',C.teal],['Periodic audit','Coverage status','An independent exhaustive check ran only when marked checked. Unchecked zero is not evidence.',C.amber]];
for(let i=0;i<3;i++){const x=62+i*396;text(s,scope[i][0].toUpperCase(),x,187,353,33,20,scope[i][3],true);text(s,scope[i][1],x,244,353,78,30,scope[i][3],true);text(s,scope[i][2],x,354,353,181,25);}
rect(s,62,558,1150,2,C.line);
text(s,'A HUD value is one step. A benchmark chart summarizes a specified time window.',62,585,1150,54,27,C.ink,true);
note(s,'Metrics::pairs/candidatePairs describe the latest broad-phase query. contacts/interSheetContacts/sphereContacts/selfContacts accumulate successful correction applications over a complete timestep. They are neither unique pair counts nor contact probabilities. Do not divide counters with different scopes. oracleChecked must be true before an oracleMisses=0 value is interpreted as coverage evidence. Native screenshots are individual frames; chart means use declared warm-up and measurement windows.');

s=slide('Measure coverage, physics and cost separately','EXPERIMENT DESIGN');
point(s,'Controlled comparison','1/2/3 sheets × seven policies; identical forces, constraints and timestep. Three local repeats.',181);
point(s,'Independent checks','Compare every particle against SAP; periodically audit candidates with exhaustive AABB search.',313,C.blue);
point(s,'Timing boundary','Complete CPU step includes periodic oracle checks; rendering is excluded. Isolated AABB timing is separate.',445,C.amber);
text(s,'Δt = 1/90 s · 5 solver iterations · 60 steps/policy (warm-up: 8)\nOnline trace: 160 steps (warm-up: 12). Same settings across policies.',62,580,1155,62,23,C.muted);
note(s,'Raw records after/multicloth-{1,2,3}.csv (63 aggregate rows) and after/online-{1,2,3}.csv (480 per-frame rows). Apple M2 Pro, AppleClang Release; metadata records source/executable hashes. Repeats are on one deterministic scene family, not independent scene samples. Method execution order is fixed in the seven-way comparison; timing-guided controller decisions can vary. Confidence intervals are not claimed.');

s=slide('Online improves on SAP here; global guard wins','LOCAL RESULTS · THREE CLOTHS');
await image(s,assets+'/policies.png',58,166,835,470,'Seven-policy full CPU step comparison, three local repeat means with observed trial-mean ranges');
const q=results['3'];
text(s,`${((q.sap_rebuild.mean-q.online_contact_work.mean)/q.sap_rebuild.mean*100).toFixed(1)}% below SAP`,934,207,282,101,34,C.teal,true);
text(s,'Online reduces full step\ntime in this fixture.',934,330,282,95,25);
text(s,'Global guard is faster',934,439,282,67,28,C.amber,true);
text(s,'Adaptation overhead can\noutweigh local pruning on\na small scene.',934,525,282,75,22,C.muted);
text(s,'Bars: mean of three trial means. Whiskers: min–max trial means, not confidence intervals.',62,615,1150,29,20,C.muted);
note(s,'Computed directly from fresh after/multicloth-{1,2,3}.csv, sheets=3, 52 timed steps per variant/trial. Full CPU timings include periodic audit costs. All 63 rows have maxPositionError=0. Online is not the fastest tested policy; small-fixture overhead can outweigh local pruning. Do not attribute this chart to an IPC/OGC adapter, GPU kernel or general speedup.');

s=slide('The winning policy balances search and response','MEASURED STAGE BREAKDOWN · SAME THREE-CLOTH RUNS');
await image(s,d+'/stage-cost.png',58,171,816,425,'Mean complete CPU step split into measured broad-phase, precise response and residual other CPU costs');
text(s,'Global guard vs SAP',924,188,298,78,28,C.amber,true);
text(s,`Broad phase\n${q.global_guarded.broad.toFixed(2)} vs ${q.sap_rebuild.broad.toFixed(2)} ms`,924,294,298,94,27,C.ink,true);
text(s,`Precise response\n${q.global_guarded.narrow.toFixed(2)} vs ${q.sap_rebuild.narrow.toFixed(2)} ms`,924,419,298,94,27,C.ink,true);
text(s,'Search savings exceed\nthe extra precise work.',924,545,298,65,23,C.teal,true);
text(s,'Other CPU = total − broad − precise; includes constraints, periodic audit and controller work. Rendering excluded.',62,616,1150,32,20,C.muted);
note(s,'Derived from the same three paired repetitions as the total-cost chart, not a separate benchmark. Each component is the mean of its per-trial means. Other CPU is an arithmetic residual from measured total/broad/narrow columns; it is not a separately instrumented kernel. The observed global-guard saving in broad time exceeds its increase in precise-response time. This explains the measured decomposition in this fixture, without claiming a universal winner or causal ablation of every component.');

s=slide('The trace shows real structural changes','ONLINE CONTROL · FIRST OF THREE REPEATS');
await image(s,assets+'/online-trace.png',58,165,856,474,'Online and SAP time series with actual region leaf counts over160 steps');
const tr=results.trace[0];text(s,`${tr.splits} splits · ${tr.switches} switches`,948,190,270,112,29,C.teal,true);
text(s,'Rising leaf count means\nmore collision regions,\nnot a finer cloth mesh.',948,331,270,114,24);
text(s,`${tr.merges} observed merges`,948,477,270,49,25,C.amber,true);
text(s,'Merge tested separately.\nSpikes include audits.',948,545,270,75,22,C.muted);
note(s,'after/online-1.csv. Blue vertical markers indicate actual split frames; region count grows 12 to 48. This repeat performs 12 splits, zero merges, four strategy switches. All three repeats compare every frame with SAP with maximum error0 and audit11 frames each. The time spikes include periodic oracle work. Selection windows do not provide same-frame A/B causal proof.');


s=slide('Audit fixes make the evidence harder to misread','CODE SAFETY AND REDUNDANCY');
table(s,[['Found','Changed','Verification'],['Finite inputs could overflow padding','Validate padded bounds and guards before cache use','Overflow + recovery regression'],['Redundant pair pass / dead helpers','Remove proven redundant unique pass and unused code','Multi-region uniqueness + equivalence'],['tee could hide failed benchmark','Bash pipefail; reject incomplete/invalid CSVs','Malformed-data and shell fixtures'],['Native trace / HUD ambiguity','Per-step scene, policy, grab and audit flags; fit HUD','Window captures and CLI checks']],60,180,1160,363,[365,440,355],22);
text(s,'Seven Release test suites pass. The 480-step stability probe remains bounded.',62,582,1150,51,27,C.teal,true);
note(s,'Exact changes and commands: CODE_AUDIT_CORE.md, EVIDENCE_METHODS_AUDIT.md, NATIVE_CASES.md and output/review-2026-11-02/VERIFICATION.md. Removal of one redundant pass is an optimization; no statistically established code-audit speedup is claimed. Sanitizer results are recorded separately. Native finite-state checks pause runaway motion.');

s=slide('What the present evidence does not establish','LIMITATIONS AND CONSEQUENCES');
const limits=[['Contact safety','Discrete checks can miss fast crossings. AABB coverage does not prove continuous nonpenetration.',C.amber],['Physical model','480-step motion stays bounded, but damping and 12,105 velocity clamps affect the trajectory. Material accuracy remains uncalibrated.',C.teal],['Generalization','One small cloth family, fixed resolution, CPU code and heuristic control. No official IPC / OGC / CCFR adapter.',C.blue]];
for(let i=0;i<3;i++){const x=62+396*i;rect(s,x,196,352,3,limits[i][2]);text(s,limits[i][0],x,232,352,62,31,limits[i][2],true);text(s,limits[i][1],x,343,352,208,25);}
text(s,'Matching trajectories protect this comparison; they do not certify physical accuracy.',62,594,1150,55,27,C.ink,true);
note(s,'after/stability.log: max absolute coordinate2.45379, max frame displacement.08324, structural edge stretch1.14950, floor minZ.04000, velocityClamps12105 over480frames. These are implementation safeguards and engineering bounds, not physical calibration. No general historical originality or solver-independent performance claim.');


s=slide('Three tests to strengthen the proposal','NEXT RESEARCH STEPS');
const next=[['01','Challenge the policy','Larger independent scenes, randomized execution order, and controller / stage-cost ablations.','Does the benefit generalize?',C.teal],['02','Integrate one method','Preserve the method’s solver, narrow phase and query requirements; replace candidate generation only.','Does the adapter preserve its contract?',C.blue],['03','Extend motion safety','Explicit swept bounds, robust CCD, and adversarial tests for folding and floating-point errors.','Can the safety scope be expanded?',C.amber]];
for(let i=0;i<3;i++){const x=62+396*i;text(s,next[i][0],x,184,352,65,47,next[i][4],true);text(s,next[i][1],x,282,352,80,30,C.ink,true);text(s,next[i][2],x,393,352,148,25);text(s,next[i][3],x,585,352,59,22,next[i][4],true);}
note(s,'Prioritize comparison to persistent dynamic BVHs and automatic pruning on compatible workloads, not only rebuild baselines. A true external adapter must preserve method-specific motion/offset requirements. Scalable CCD provides relevant conservative algorithms and benchmark data: '+src.ccd);


s=slide('Sources and reproducible evidence','REFERENCES');
text(s,'IPC — Li et al., 2020 · Algorithm 1, Table 1, Fig. 23\nOGC — Chen et al., 2025 · Table 1, Figs. 17–18\nXPBD — Macklin et al., 2016 · compliant constraint formulation\nCCFR — Kikuchi & Kanai, 2026 · §4.3, Table 1, Figs. 10–11',62,185,1150,199,26);
text(s,'Metric choices: full runtime and collision-stage cost; contact workload;\nmethod-specific convergence and remaining violations.',62,419,1130,85,28,C.teal,true);
text(s,'Local bundle: output/review-2026-11-02/\nRaw CSVs · full native captures · logs · source/build hashes · verification report',62,548,1140,72,24,C.muted);
note(s,Object.entries(src).map(([k,v])=>k+': '+v).join('\n')+'\nEvaluation anchors refer to quantities used in the papers, not numerical comparisons against them. OGC publication/project page checked; source paper location retained from earlier audit. All demo figures come from actual local C++ measurements or native framebuffers. No paper result image is substituted for implemented output.');

await fs.mkdir(d+'/renders',{recursive:true});
for(let i=0;i<p.slides.items.length;i++){const b=await p.export({slide:p.slides.items[i],format:'png',scale:1});await fs.writeFile(d+`/renders/slide-${String(i+1).padStart(2,'0')}.png`,new Uint8Array(await b.arrayBuffer()));}
await (await PresentationFile.exportPptx(p)).save(d+'/candidate.pptx');console.log('Rendered',p.slides.items.length,'research slides');
