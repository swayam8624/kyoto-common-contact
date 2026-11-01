from pathlib import Path
p=Path(__file__).with_name('build.mjs');s=p.read_text()
def replace(start,end,body,keep_notes=True):
 global s
 a=s.index(start);b=s.index(end,a);old=s[a:b]
 notes=old[old.index('note(s,'):] if keep_notes else ''
 s=s[:a]+body+'\n'+notes+'\n'+s[b:]
replace("let s=slide(","s=slide('The question",r'''let s=slide('One broad phase, adapting to contact work','RESEARCH PROPOSAL · VERIFIED C++ PROTOTYPE');
text(s,'Share candidate infrastructure.\nKeep each method’s contact rules.',62,193,655,125,39,C.ink,true);
text(s,'The prototype changes collision-search regions\nusing measured work and confirmed corrections.',62,357,650,107,29,C.muted);
text(s,'One geometric format\nCertified candidate reuse\nOnline partition decisions',62,510,640,115,27,C.teal,true);
await image(s,ev+'/cases/one-sap-scene.png',760,173,460,421,'Actual native OpenGL cloth scene; crop from the one-sheet SAP framebuffer');
text(s,'Actual native C++ scene · one cloth case',760,604,460,30,18,C.muted);''')
replace("s=slide('The question","s=slide('Contact methods",r'''s=slide('Less search does not always mean a faster step','PROBLEM AND OBJECTIVE');
text(s,'REUSE LARGER GUARDS',62,193,535,39,25,C.teal,true);
text(s,'Fewer candidate-list rebuilds.\nMore stale pairs may reach filtering\nand precise contact checks.',62,252,535,146,31);
text(s,'USE SMALLER REGIONS',690,193,527,39,25,C.blue,true);
text(s,'More local pruning.\nMore region bookkeeping, guard\nchecks and cache maintenance.',690,252,527,146,31);
rect(s,62,435,1150,2,C.line);
text(s,'Optimize complete collision cost',62,476,1150,48,33,C.ink,true);
text(s,'Measure search + candidate filtering + precise response + controller overhead.\nA smaller candidate set alone is not the objective.',62,548,1150,86,27,C.muted);''')
replace("s=slide('Established ideas","s=slide('One pipeline",r'''s=slide('The research question is the control policy','ESTABLISHED METHODS → PROPOSED EVALUATION');
text(s,'ESTABLISHED MACHINERY',62,189,535,38,24,C.muted,true);
text(s,'Spatial hierarchies and sweeps\nFat bounds and temporal reuse\nAutomatic box pruning',62,250,535,166,31);
text(s,'OUR IMPLEMENTED POLICY',691,189,525,38,24,C.teal,true);
text(s,'Observe local work and corrections\nAdjust mesh-patch membership\nRebuild certificates before reuse',691,250,525,166,31);
rect(s,62,456,1150,2,C.line);
text(s,'Does contact-work feedback reduce total cost\nagainst strong baselines as the scene changes?',62,490,1130,96,34,C.ink,true);
text(s,'Policy novelty and transfer across solvers remain unproven. Comparators include Bullet DBVT and PhysX ABP/PABP.',62,608,1150,36,20,C.muted);''')
replace("s=slide('One pipeline","s=slide('A reuse certificate",r'''s=slide('The shared stage sits inside the solver loop','PIPELINE AND OWNERSHIP');
const stages=[['Predict / constrain','Cloth positions and\ninternal constraints','CALLER'],['Prepare bounds','Raw min/max boxes\nand eligibility','CALLER'],['Find candidates','Certified regional\nsearch and reuse','SHARED SERVICE'],['Precise response','Vertex–face, edge–edge\nand sphere checks','CALLER']];
for(let i=0;i<4;i++){const x=62+i*296;text(s,stages[i][2],x,190,264,32,17,i===2?C.teal:C.muted,true);rect(s,x,239,260,4,i===2?C.teal:C.line);text(s,stages[i][0],x,271,261,82,29,i===2?C.teal:C.ink,true);text(s,stages[i][1],x,370,260,103,25,C.muted);if(i<3)text(s,'→',x+264,300,38,50,31,C.blue,true);}
rect(s,62,503,1150,2,C.line);
text(s,'Five collision queries per timestep; one controller update at the end.',62,535,1150,49,30,C.ink,true);
text(s,'Candidate IDs enter the same precise contact solver. The renderer reads the resulting C++ positions.',62,604,1150,38,23,C.teal);''')
# A new contract slide explains the data boundary before the certificate.
a=s.index("s=slide('A reuse certificate")
s=s[:a]+r'''s=slide('What crosses the common interface?','SHARED GEOMETRIC FORMAT');
const cols=[62,458,854];
text(s,'GEOMETRY',cols[0],187,360,36,23,C.blue,true);
text(s,'Stable primitive ID\nMinimum / maximum XYZ\nCategory and collision mask\nMovable or fixed flag',cols[0],247,350,212,28);
text(s,'QUERY REQUIREMENTS',cols[1],187,360,36,23,C.teal,true);
text(s,'Proximity margin\nReuse / rebuild policy\nGuard displacement budget\nCurrent-configuration scope',cols[1],247,350,212,28);
text(s,'CANDIDATE OUTPUT',cols[2],187,360,36,23,C.amber,true);
text(s,'Canonical primitive-ID pairs\nPossible interactions only\nPrecise testing still required',cols[2],247,350,170,28);
rect(s,62,488,1150,2,C.line);
text(s,'AABB = an axis-aligned min/max box around a primitive.',62,522,1150,42,27,C.ink,true);
text(s,'The caller supplies conservative geometry and contact semantics;\nthe shared service does not select forces or replace the solver.',62,582,1150,61,25,C.muted);
note(s,'Existing common record Bounds{id,min,max,category,mask,movable}. The global service uses QueryRequest{margin,displacementBudget,updatePolicy}; the regional path uses the same geometric record plus region/sheet metadata, margin and policy controls. This normalizes the conceptual boundary, not a claim that official IPC/OGC adapters already implement one universal API. Half the margin pads each primitive box. Current queries are discrete, not swept trajectories.');

'''+s[a:]
replace("s=slide('A reuse certificate","s=slide('Online feedback",r'''s=slide('Reuse is justified by enclosure, not prediction','CERTIFICATE AND INVALIDATION');
await image(s,assets+'/certificate.png',58,154,1165,127,'Current padded bounds contained in cached envelopes imply current overlaps are included in cached pairs');
text(s,'B: current padded box   ·   E: cached guard   ·   C: overlap-pair set',62,278,1150,35,22,C.muted);
text(s,'STAYS INSIDE THE GUARD',62,347,535,36,23,C.teal,true);
text(s,'Reuse the cached pair superset.\nRecheck each pair against today’s\ntight padded boxes.',62,404,535,136,30);
text(s,'ESCAPES OR MEANING CHANGES',690,347,528,36,23,C.amber,true);
text(s,'Rebuild before answering.\nA changed mask, request or patch\nassignment also invalidates reuse.',690,404,528,136,30);
rect(s,62,568,1150,2,C.line);
text(s,'Why it works: an overlap inside smaller boxes must also exist in their enclosing guards.',62,590,1150,39,24,C.ink,true);
text(s,'This is a current-configuration coverage argument; it does not certify continuous motion.',62,630,1150,23,18,C.muted);''')
replace("s=slide('Online feedback","s=slide('Every comparison",r'''s=slide('Measured work drives bounded online decisions','RUNTIME CONTROL LOOP');
const flow=[['Observe','Box tests, rechecks,\ninvalidations and corrections'],['Smooth','Reduce reaction to\nsingle-step noise'],['Decide','Six-frame windows\nplus cooldowns'],['Apply','Patch edits or traversal\nchange; reset certificates']];
for(let i=0;i<4;i++){const x=62+296*i;rect(s,x,194,259,3,i===3?C.teal:C.line);text(s,flow[i][0],x,219,260,50,31,C.teal,true);text(s,flow[i][1],x,291,260,110,24);if(i<3)text(s,'→',x+265,231,33,45,28,C.blue,true);}
text(s,'Split hot patches',62,439,535,44,29,C.blue,true);
text(s,'Localize expensive searches and rechecks.\nNew certificates cost work immediately.',62,498,535,98,26);
text(s,'Merge quiet siblings',690,439,525,44,29,C.amber,true);
text(s,'Reduce region bookkeeping when little\ncontact work is being observed.',690,498,525,98,26);
text(s,'The 12 → 6 → 3-cell hierarchy changes region IDs, not particles or physical constraints. Thresholds are heuristic.',62,615,1150,34,21,C.muted);''')
# Native screenshots get larger image space and more explanatory captions.
replace("s=slide('Native case:","s=slide('A matched native",r'''s=slide('Native evidence: the online policy is in the loop','THREE CLOTHS · TWO MOVING SPHERES · STEP 160');
await image(s,ev+'/cases/three-online.png',40,159,875,480,'Full actual native OpenGL framebuffer at step160 using the online policy');
text(s,'What this establishes',944,184,279,86,29,C.teal,true);
text(s,'The running C++ solver\nproduces the displayed mesh\nand contact corrections.',944,300,279,136,25);
text(s,'Self-contact is enabled.\nThis frame does not show\na positive self-contact event.',944,476,279,116,23,C.muted);
text(s,'Full framebuffer, selected policy, configuration and per-step CSV are retained.',62,626,1150,24,18,C.muted);''')
replace("s=slide('A matched native","s=slide('Measure coverage",r'''s=slide('Matched state; different candidate maintenance','SAP REBUILD REFERENCE · SAME STEP 160');
await image(s,ev+'/cases/three-sap.png',40,159,875,480,'Full native SAP reference framebuffer at the same step160');
text(s,'What is held fixed?',944,184,279,84,29,C.blue,true);
text(s,'Geometry, forces, timestep,\nprecise tests and solver.',944,290,279,95,25);
text(s,'All 160 native trace\nchecksums match the\nonline case.',944,430,279,116,27,C.teal,true);
text(s,'The headless comparison also checks every particle coordinate; a screenshot alone cannot prove equivalence.',62,624,1150,26,19,C.muted);''')
a=s.index("s=slide('Measure coverage")
s=s[:a]+r'''s=slide('Read the HUD counters at their actual scope','METRIC INTERPRETATION');
const scope=[['Latest query','Candidates','Possible pairs returned by the last\nquery. Extra pairs are allowed.\nThey are not confirmed collisions.',C.blue],['Whole timestep','Contact corrections','Executed positional corrections\nacross solver iterations. One pair\ncan contribute more than once.',C.teal],['Periodic audit','Coverage status','An exhaustive independent check\nran only when marked checked.\nUnchecked zero is not evidence.',C.amber]];
for(let i=0;i<3;i++){const x=62+i*396;text(s,scope[i][0].toUpperCase(),x,187,353,33,20,scope[i][3],true);text(s,scope[i][1],x,244,353,78,30,scope[i][3],true);text(s,scope[i][2],x,354,353,181,25);}
rect(s,62,558,1150,2,C.line);
text(s,'A HUD value is one step. A benchmark chart summarizes a specified time window.',62,585,1150,54,27,C.ink,true);
note(s,'Metrics::pairs/candidatePairs describe the latest broad-phase query. contacts/interSheetContacts/sphereContacts/selfContacts accumulate successful correction applications over a complete timestep. They are neither unique pair counts nor contact probabilities. Do not divide counters with different scopes. oracleChecked must be true before an oracleMisses=0 value is interpreted as coverage evidence. Native screenshots are individual frames; chart means use declared warm-up and measurement windows.');

'''+s[a:]
# Replace duplicate numeric callouts with interpretation alongside the existing plot.
a=s.index("const q=results['3'];");b=s.index("text(s,'Bars: mean",a)
s=s[:a]+r'''const q=results['3'];
text(s,`${((q.sap_rebuild.mean-q.online_contact_work.mean)/q.sap_rebuild.mean*100).toFixed(1)}% below SAP`,934,207,282,101,34,C.teal,true);
text(s,'Online reduces full step\ntime in this fixture.',934,330,282,95,25);
text(s,'Global guard is faster',934,459,282,75,28,C.amber,true);
text(s,'Adaptation overhead can\noutweigh local pruning on\na small scene.',934,548,282,91,22,C.muted);
'''+s[b:]
# New measured stage chart explains why global guard can win.
a=s.index("s=slide('The controller changes")
s=s[:a]+r'''s=slide('The winning policy balances search and response','MEASURED STAGE BREAKDOWN · SAME THREE-CLOTH RUNS');
await image(s,d+'/stage-cost.png',58,171,816,425,'Mean complete CPU step split into measured broad-phase, precise response and residual other CPU costs');
text(s,'Global guard vs SAP',924,188,298,78,28,C.amber,true);
text(s,`Broad phase\n${q.global_guarded.broad.toFixed(2)} vs ${q.sap_rebuild.broad.toFixed(2)} ms`,924,294,298,94,27,C.ink,true);
text(s,`Precise response\n${q.global_guarded.narrow.toFixed(2)} vs ${q.sap_rebuild.narrow.toFixed(2)} ms`,924,419,298,94,27,C.ink,true);
text(s,'The broad-phase saving exceeds\nthe extra precise work here.',924,548,298,76,23,C.teal,true);
text(s,'Other CPU = total − broad − precise; includes constraints, periodic audit and controller work. Rendering excluded.',62,616,1150,32,20,C.muted);
note(s,'Derived from the same three paired repetitions as the total-cost chart, not a separate benchmark. Each component is the mean of its per-trial means. Other CPU is an arithmetic residual from measured total/broad/narrow columns; it is not a separately instrumented kernel. The observed global-guard saving in broad time exceeds its increase in precise-response time. This explains the measured decomposition in this fixture, without claiming a universal winner or causal ablation of every component.');

'''+s[a:]
replace("s=slide('The controller changes","s=slide('Audit fixes",r'''s=slide('The trace shows real structural changes','ONLINE CONTROL · FIRST OF THREE REPEATS');
await image(s,assets+'/online-trace.png',58,165,856,474,'Online and SAP time series with actual region leaf counts over160 steps');
const tr=results.trace[0];text(s,`${tr.splits} splits · ${tr.switches} switches`,948,190,270,112,29,C.teal,true);
text(s,'Rising leaf count means\nmore collision regions,\nnot a finer cloth mesh.',948,331,270,114,24);
text(s,`${tr.merges} observed merges`,948,477,270,49,25,C.amber,true);
text(s,'Merging has a separate test.\nTiming spikes include audit work.',948,553,270,85,22,C.muted);''')
replace("s=slide('Coverage is necessary","s=slide('Next experiments",r'''s=slide('What the present evidence does not establish','LIMITATIONS AND CONSEQUENCES');
const limits=[['Contact safety','Discrete contact can tunnel under\nfast folds. Current AABB coverage\nis not a continuous nonpenetration\nguarantee.',C.amber],['Physical model','The 480-step probe stays bounded,\nbut damping and 12,105 velocity\nclamps affect motion. Material\naccuracy is not calibrated.',C.teal],['Generalization','One small fixed-resolution cloth\nfamily; CPU implementation and\nheuristic control. No official\nIPC / OGC / CCFR adapter.',C.blue]];
for(let i=0;i<3;i++){const x=62+396*i;rect(s,x,196,352,3,limits[i][2]);text(s,limits[i][0],x,232,352,62,31,limits[i][2],true);text(s,limits[i][1],x,343,352,208,25);}
text(s,'Matching trajectories protect this comparison; they do not certify physical accuracy.',62,594,1150,55,27,C.ink,true);''')
replace("s=slide('Next experiments","s=slide('Sources and",r'''s=slide('Three experiments would strengthen the proposal','NEXT RESEARCH STEPS');
const next=[['01','Challenge the policy','Larger independent scenes;\nrandomized execution order;\ncontroller and stage-cost ablations.','Does the benefit generalize?',C.teal],['02','Integrate one method','Preserve its narrow phase, solver,\nmargins and motion requirements;\nreplace candidate generation only.','Does the adapter preserve its contract?',C.blue],['03','Extend motion safety','Explicit swept-bound requests,\nrobust CCD and adversarial\nfolding / floating-point tests.','Can the safety scope be expanded?',C.amber]];
for(let i=0;i<3;i++){const x=62+396*i;text(s,next[i][0],x,184,352,65,47,next[i][4],true);text(s,next[i][1],x,282,352,80,30,C.ink,true);text(s,next[i][2],x,393,352,148,25);text(s,next[i][3],x,585,352,59,22,next[i][4],true);}''')
p.write_text(s)
