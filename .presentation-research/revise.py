from pathlib import Path
p=Path('.presentation-rebuild/build.mjs');s=p.read_text()
s=s.replace("text(s,'One record format. One controlled geometry experiment.\\nA concrete interface to discuss with Dr. Tomoyo Kikuchi.'", "text(s,'Research proposal · conservative candidate generation\\nA shared interface evaluated against each method’s native pipeline.'")
a=s.index("s=slide('One experiment");b=s.index("s=slide('How existing methods",a)
s=s[:a]+'''s=slide('Pilot result: candidates require a precise check');
text(s,'A   Candidate retained, precise check rejects',64,205,560,60,27,amber,true);
text(s,'B   Candidate retained, within the margin',660,205,560,60,27,green,true);
s.images.add({svg:await fs.readFile(root+'/.presentation-research/false-positive.svg','utf8'),alt:'Computed C++ pilot state: overlapping padded bounds with distance greater than margin',position:{left:64,top:260,width:550,height:290},fit:'contain'});
s.images.add({svg:await fs.readFile(root+'/.presentation-research/within-margin.svg','utf8'),alt:'Computed C++ pilot state: distance within margin and candidate retained',position:{left:660,top:260,width:550,height:290},fit:'contain'});
text(s,'d > δ: an allowed false positive.\\nBroad phase cannot decide contact.',64,560,550,78,27);
text(s,'d ≤ δ: the pair reaches precise handling.\\nRetention is required by this query.',660,560,550,78,27);
s.speakerNotes.text='Figures regenerated from actual C++ snapshot data embedded in output/Common_Broad_Phase_Demo.html, not screenshots of external methods. Both panels show the same vertex-triangle trajectory and δ. Blue rectangles are the triangle bounds; the small rectangle and point indicate the moving vertex. The XY view is a projection; the vertex is above z=0. These are pilot geometric results, not solver benchmark results. State A is a false positive; state B demonstrates necessary pair retention. No sampled-state aggregate counts are reported.';
s=slide('Evaluation measures from the contact literature');
text(s,'PROPOSED EVALUATION · measurements to collect after integration',64,195,1150,35,20,muted,true);
const metrics=[
 ['Runtime','Average / maximum step time;\\ncollision-stage time breakdown','CCFR Table 1, Fig. 10\\nOGC Table 1'],
 ['Collision workload','Candidate / contact workload;\\nnumber of collision detections','IPC Fig. 23\\nOGC Fig. 18'],
 ['Convergence','Iterations and residual vs time,\\nwhen solver measures are comparable','IPC Fig. 23\\nOGC Fig. 17'],
 ['Collision safety','Remaining violations or intersections\\nafter collision handling','CCFR Fig. 11\\nOGC §5']
];
for(let i=0;i<metrics.length;i++){const y=255+i*91;line(s,64,y-10,1152);text(s,metrics[i][0],64,y,260,70,29,ink,true);text(s,metrics[i][1],340,y,570,78,26);text(s,metrics[i][2],970,y,245,75,21,muted)}
s.speakerNotes.text='These are evaluation quantities used by the cited work, not measurements of this prototype. CCFR: Kikuchi & Kanai, Convex Collision-Free Regions, §4.3 Table1, Figs10–11, https://arxiv.org/html/2607.26901v1 . OGC: Chen et al., Offset Geometric Contact, Table1, §5, §5.3 Fig17, §5.4 Fig18, https://graphics.cs.utah.edu/research/projects/ogc/Offset_Geometric_Contact-SIGGRAPH2025.pdf . IPC: Li et al., Incremental Potential Contact, Fig23 and Table1, https://ipc-sim.github.io/file/IPC-paper-fullRes.pdf . CCFR reports aggregate broad-phase time; splitting build and traversal is a proposed instrumentation extension. OGC checks intersections after every frame. Convergence quantities are method-dependent; do not equate residuals across unlike solvers.';
'''+s[b:]
a=s.index("s=slide('The next experiment")
s=s[:a]+'''s=slide('Controlled comparison: native vs shared candidates');
text(s,'CHANGE ONE COMPONENT',64,210,550,35,21,blue,true);text(s,'Native candidate generator\\nversus\\nshared bounds-based generator',64,270,520,170,34,blue,true);
text(s,'HOLD THE SIMULATION FIXED',690,210,520,35,21,muted,true);text(s,'Same geometry and trajectory\\nSame timestep and contact settings\\nSame narrow phase and solver\\nSame hardware and stopping rules',690,270,530,220,29);
text(s,'Compare identical simulated duration; report workload size and settings.\\nA faster broad phase is useful only if collision safety is preserved.',64,545,1140,90,29);
s.speakerNotes.text='Proposed paired experiment, not completed. Inspired by IPC Table1/Fig23 timestep and workload reporting, OGC Table1 workload/time comparisons, and CCFR Table1 stage breakdown. Integrate and validate one codebase first, then repeat the same comparison independently for another method. Never attribute an end-to-end difference to the broad phase if solver settings differ.';
s=slide('Measures specific to the shared broad phase');
text(s,'Correctness',64,215,330,60,33,ink,true);text(s,'Required-pair omissions against an exhaustive oracle\\nEligibility, margins and time interval must match.',400,215,810,110,29);
text(s,'Cost',64,355,330,60,33,ink,true);text(s,'Construction / update time, query time, peak candidates\\nTrack total collision-handling time as well.',400,355,810,110,29);
text(s,'Sensitivity',64,495,330,60,33,ink,true);text(s,'Vary primitive count, contact density and motion range\\nRepeat at the method’s supported timestep and margin.',400,495,810,110,29);
s.speakerNotes.text='These are proposed infrastructure diagnostics adapted for this research question. Candidate coverage vs an exhaustive oracle is not presented as a metric reported in IPC/OGC/CCFR. Candidate and stage-cost analysis is motivated by CCFR §4.3 Figs10–11 and OGC §5.4 Fig18. No timing campaign, speedup, solver equivalence or cross-method numerical result has been measured by this prototype.';
'''+s[a:]
s=s.replace("/.presentation-rebuild/renders", "/.presentation-research/renders").replace("/.presentation-rebuild/candidate.pptx", "/.presentation-research/candidate.pptx")
Path('.presentation-research/build.mjs').write_text(s)
