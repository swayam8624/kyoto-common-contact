#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using Vec3 = std::array<double, 3>;

struct PrimitiveBounds {
    int id;
    std::string kind;
    Vec3 min;
    Vec3 max;
};

struct Pair {
    int a;
    int b;
    friend bool operator==(const Pair&, const Pair&) = default;
};

struct Triangle {
    int id;
    std::array<Vec3, 3> vertices;
};

struct Sample {
    double t;
    Vec3 point;
    double distance;
    bool candidate;
};

constexpr double kDelta = 0.12;
constexpr int kSampleCount = 81;

Vec3 add(const Vec3& a, const Vec3& b) {
    return {a[0] + b[0], a[1] + b[1], a[2] + b[2]};
}
Vec3 subtract(const Vec3& a, const Vec3& b) {
    return {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
}
Vec3 scale(const Vec3& a, double s) { return {a[0] * s, a[1] * s, a[2] * s}; }
double dot(const Vec3& a, const Vec3& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
double norm(const Vec3& a) { return std::sqrt(dot(a, a)); }

PrimitiveBounds boundsFor(int id, const std::string& kind, const std::vector<Vec3>& points,
                          double padding = 0.0) {
    PrimitiveBounds out{id, kind, points.front(), points.front()};
    for (const auto& p : points) {
        for (int axis = 0; axis < 3; ++axis) {
            out.min[axis] = std::min(out.min[axis], p[axis]);
            out.max[axis] = std::max(out.max[axis], p[axis]);
        }
    }
    for (int axis = 0; axis < 3; ++axis) {
        out.min[axis] -= padding;
        out.max[axis] += padding;
    }
    return out;
}

bool overlaps(const PrimitiveBounds& a, const PrimitiveBounds& b) {
    for (int axis = 0; axis < 3; ++axis)
        if (a.max[axis] < b.min[axis] || b.max[axis] < a.min[axis]) return false;
    return true;
}

std::vector<Pair> exhaustive(const std::vector<PrimitiveBounds>& bounds) {
    std::vector<Pair> pairs;
    for (std::size_t i = 0; i < bounds.size(); ++i)
        for (std::size_t j = i + 1; j < bounds.size(); ++j)
            if (overlaps(bounds[i], bounds[j]))
                pairs.push_back({std::min(bounds[i].id, bounds[j].id), std::max(bounds[i].id, bounds[j].id)});
    std::sort(pairs.begin(), pairs.end(), [](Pair a, Pair b) { return a.a == b.a ? a.b < b.b : a.a < b.a; });
    return pairs;
}

std::vector<Pair> sweepAndPrune(const std::vector<PrimitiveBounds>& bounds) {
    std::vector<std::size_t> order(bounds.size());
    for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;
    std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        return bounds[a].min[0] == bounds[b].min[0] ? bounds[a].id < bounds[b].id : bounds[a].min[0] < bounds[b].min[0];
    });
    std::vector<Pair> pairs;
    for (std::size_t i = 0; i < order.size(); ++i) {
        const auto ia = order[i];
        for (std::size_t j = i + 1; j < order.size() && bounds[order[j]].min[0] <= bounds[ia].max[0]; ++j) {
            const auto ib = order[j];
            if (overlaps(bounds[ia], bounds[ib]))
                pairs.push_back({std::min(bounds[ia].id, bounds[ib].id), std::max(bounds[ia].id, bounds[ib].id)});
        }
    }
    std::sort(pairs.begin(), pairs.end(), [](Pair a, Pair b) { return a.a == b.a ? a.b < b.b : a.a < b.a; });
    return pairs;
}

double pointTriangleDistance(const Vec3& p, const Triangle& tri) {
    // Closest-point region tests from the triangle Voronoi partition, followed by Euclidean distance.
    const Vec3 a = tri.vertices[0], b = tri.vertices[1], c = tri.vertices[2];
    const Vec3 ab = subtract(b, a), ac = subtract(c, a), ap = subtract(p, a);
    const double d1 = dot(ab, ap), d2 = dot(ac, ap);
    if (d1 <= 0.0 && d2 <= 0.0) return norm(ap);
    const Vec3 bp = subtract(p, b);
    const double d3 = dot(ab, bp), d4 = dot(ac, bp);
    if (d3 >= 0.0 && d4 <= d3) return norm(bp);
    const double vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0 && d1 >= 0.0 && d3 <= 0.0) return norm(subtract(p, add(a, scale(ab, d1 / (d1 - d3)))));
    const Vec3 cp = subtract(p, c);
    const double d5 = dot(ab, cp), d6 = dot(ac, cp);
    if (d6 >= 0.0 && d5 <= d6) return norm(cp);
    const double vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0 && d2 >= 0.0 && d6 <= 0.0) return norm(subtract(p, add(a, scale(ac, d2 / (d2 - d6)))));
    const double va = d3 * d6 - d5 * d4;
    if (va <= 0.0 && (d4 - d3) >= 0.0 && (d5 - d6) >= 0.0) {
        const Vec3 bc = subtract(c, b);
        return norm(subtract(p, add(b, scale(bc, (d4 - d3) / ((d4 - d3) + (d5 - d6))))));
    }
    const double inv = 1.0 / (va + vb + vc);
    const double v = vb * inv, w = vc * inv;
    return norm(subtract(p, add(a, add(scale(ab, v), scale(ac, w)))));
}

std::string number(double value) {
    std::ostringstream out;
    out << std::setprecision(10) << value;
    return out.str();
}

void validateDistanceWitnesses() {
    constexpr Triangle tri{2, {{{-0.48, -0.42, 0.0}, {0.48, -0.42, 0.0}, {0.0, 0.52, 0.0}}}};
    const auto check = [&](const Vec3& point, double expected, const char* label) {
        const double actual = pointTriangleDistance(point, tri);
        if (std::abs(actual - expected) > 1e-10)
            throw std::runtime_error(std::string("point-triangle distance witness failed: ") + label + " (got " + number(actual) + ")");
    };
    check({0.0, 0.0, 0.08}, 0.08, "projection inside face");
    check({0.0, -0.42, 0.08}, 0.08, "projection on horizontal edge");
    check({-0.48, -0.42, 0.08}, 0.08, "projection on vertex");
    check({-0.58, -0.42, 0.0}, 0.10, "outside beyond vertex");
    std::cout << "Exact-distance validation: 4 analytic point/triangle witnesses passed (face, edge, vertex, exterior vertex).\n";
}

std::string vectorJson(const Vec3& v) {
    return "[" + number(v[0]) + "," + number(v[1]) + "," + number(v[2]) + "]";
}

std::string boundsJson(const PrimitiveBounds& b) {
    return "{\"id\":" + std::to_string(b.id) + ",\"kind\":\"" + b.kind + "\",\"min\":" +
           vectorJson(b.min) + ",\"max\":" + vectorJson(b.max) + "}";
}

std::string pairJson(const Pair& p) { return "[" + std::to_string(p.a) + "," + std::to_string(p.b) + "]"; }

struct ValidationSummary { int scenes; int boundsPerScene; std::size_t candidates; };

ValidationSummary validateGeneratedScenes() {
    unsigned state = 0x4b59544fU;
    auto random01 = [&]() {
        state = state * 1664525U + 1013904223U;
        return static_cast<double>(state >> 8U) / static_cast<double>(0x01000000U);
    };
    std::size_t total = 0;
    for (int scene = 0; scene < 12; ++scene) {
        std::vector<PrimitiveBounds> bounds;
        for (int i = 0; i < 36; ++i) {
            const Vec3 center{random01() * 8.0 - 4.0, random01() * 8.0 - 4.0, random01() * 8.0 - 4.0};
            const Vec3 half{0.04 + random01() * 0.45, 0.04 + random01() * 0.45, 0.04 + random01() * 0.45};
            bounds.push_back({i + 1, "box", {center[0] - half[0], center[1] - half[1], center[2] - half[2]},
                              {center[0] + half[0], center[1] + half[1], center[2] + half[2]}});
        }
        const auto oracle = exhaustive(bounds);
        const auto sap = sweepAndPrune(bounds);
        if (oracle != sap) throw std::runtime_error("SAP candidate mismatch in generated scene " + std::to_string(scene));
        total += oracle.size();
    }
    std::cout << "Generated-scene validation: 12 scenes, 36 bounds each; SAP equals exhaustive oracle in all scenes (" << total
              << " candidate pairs checked).\n";
    return {12, 36, total};
}

std::string makeData(const ValidationSummary& validation) {
    constexpr Triangle tri{2, {{{-0.48, -0.42, 0.0}, {0.48, -0.42, 0.0}, {0.0, 0.52, 0.0}}}};
    constexpr Vec3 start{-1.0, 0.08, 0.08}, end{1.0, 0.08, 0.08};
    const auto triangleBounds = boundsFor(tri.id, "triangle", {tri.vertices[0], tri.vertices[1], tri.vertices[2]}, kDelta / 2.0);
    const auto sweptPointBounds = boundsFor(1, "point", {start, end}, kDelta / 2.0);
    const auto intervalCandidates = sweepAndPrune({sweptPointBounds, triangleBounds});
    if (intervalCandidates != std::vector<Pair>{{1, 2}}) throw std::runtime_error("expected interval candidate was not produced");

    std::vector<Sample> samples;
    std::size_t truePairs = 0, candidates = 0, falsePositives = 0;
    std::ostringstream sampleJson;
    sampleJson << '[';
    for (int i = 0; i < kSampleCount; ++i) {
        const double t = static_cast<double>(i) / (kSampleCount - 1);
        const Vec3 p = add(start, scale(subtract(end, start), t));
        const auto pointBounds = boundsFor(1, "point", {p}, kDelta / 2.0);
        const bool candidate = !sweepAndPrune({pointBounds, triangleBounds}).empty();
        const double distance = pointTriangleDistance(p, tri);
        const bool required = distance <= kDelta + 1e-12;
        if (candidate) ++candidates;
        if (required) {
            ++truePairs;
            if (!candidate) throw std::runtime_error("required proximity pair omitted at sample " + std::to_string(i));
        }
        if (candidate && !required) ++falsePositives;
        if (i) sampleJson << ',';
        sampleJson << "{\"t\":" << number(t) << ",\"point\":" << vectorJson(p) << ",\"distance\":" << number(distance)
                   << ",\"candidate\":" << (candidate ? "true" : "false") << ",\"withinDelta\":" << (required ? "true" : "false") << '}';
        samples.push_back({t, p, distance, candidate});
    }
    sampleJson << ']';

    std::ostringstream data;
    data << "{\"delta\":" << number(kDelta) << ",\"samples\":" << sampleJson.str() << ",\"triangle\":["
         << vectorJson(tri.vertices[0]) << ',' << vectorJson(tri.vertices[1]) << ',' << vectorJson(tri.vertices[2])
         << "],\"bounds\":[" << boundsJson(sweptPointBounds) << ',' << boundsJson(triangleBounds)
         << "],\"intervalCandidates\":[" << pairJson(intervalCandidates.front()) << "],\"validation\":{\"generatedScenes\":"
         << validation.scenes << ",\"boundsPerScene\":" << validation.boundsPerScene << ",\"generatedPairs\":" << validation.candidates
         << ",\"sapMatchesOracle\":true},\"counts\":{\"samples\":" << samples.size()
         << ",\"required\":" << truePairs << ",\"candidates\":" << candidates << ",\"falsePositives\":" << falsePositives << "}}";
    std::cout << "Experiment validation: " << samples.size() << " states; " << truePairs << " sampled states within delta; "
              << candidates << " candidate states; " << falsePositives << " broad-phase false positives; all required pairs retained.\n";
    return data.str();
}

std::string html() {
    return R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Common Broad Phase: One Small Experiment</title>
<style>
:root{--blue:#2667d8;--amber:#d78300;--green:#18834b;--ink:#15191f;--muted:#596575;--line:#d8dee7;--paper:#fff;--wash:#f5f7fa}*{box-sizing:border-box}body{margin:0;background:var(--paper);color:var(--ink);font:16px/1.5 system-ui,-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif}main{max-width:1050px;margin:auto;padding:26px 24px 48px}h1{font-size:clamp(27px,4vw,42px);line-height:1.12;margin:8px 0 8px;letter-spacing:-.025em}h2{font-size:21px;margin:0 0 10px}.lede{color:#344052;max-width:790px;margin:0 0 20px}.tag{font-size:12px;letter-spacing:.09em;text-transform:uppercase;font-weight:750;color:#465365}.contract{border:1px solid var(--line);background:var(--wash);padding:14px 16px;border-radius:8px;margin:18px 0}.contract code{font-size:14px;white-space:normal}.stages{display:flex;gap:8px;margin:20px 0 12px}.stages button,.precise,.play{background:#fff;border:1px solid #aeb8c5;color:var(--ink);border-radius:6px;padding:9px 14px;font:inherit;font-weight:650;cursor:pointer}.stages button[aria-pressed=true]{border-color:var(--ink);background:var(--ink);color:white}.precise[aria-pressed=true]{border-color:var(--green);color:var(--green)}.panel{border:1px solid var(--line);border-radius:9px;padding:18px;margin:12px 0;background:white}.stage-copy{color:var(--muted);min-height:25px;margin:0 0 8px}.viz{width:100%;height:auto;display:block;background:#fbfcfe;border:1px solid #e7ebf0;border-radius:6px}.controls{display:grid;grid-template-columns:auto 1fr auto;gap:12px;align-items:center;margin-top:12px}.controls input{width:100%;accent-color:var(--blue)}.readout{display:grid;grid-template-columns:repeat(3,1fr);gap:10px;margin-top:12px}.metric{border:1px solid var(--line);border-radius:6px;padding:9px 11px}.metric span{display:block;color:var(--muted);font-size:12px}.metric strong{font-variant-numeric:tabular-nums}.status{font-weight:750}.status.yes{color:var(--green)}.status.no{color:var(--amber)}.legend{display:flex;gap:18px;flex-wrap:wrap;color:#384353;font-size:13px;margin:10px 0 0}.swatch{display:inline-block;width:12px;height:12px;margin-right:6px;vertical-align:-1px;border:2px solid}.sw-blue{border-color:var(--blue)}.sw-amber{background:#fff1d8;border-color:var(--amber)}.sw-green{background:#e2f4eb;border-color:var(--green)}.note{color:var(--muted);font-size:14px}.contract-list{margin:8px 0 0;padding-left:20px}.hidden{display:none}footer{margin-top:20px;color:var(--muted);font-size:13px}@media(max-width:600px){main{padding:18px 13px}.readout{grid-template-columns:1fr}.controls{grid-template-columns:auto 1fr}.controls output{grid-column:2}.stages{flex-wrap:wrap}}
</style></head><body><main>
<div class="tag">Offline C++23 experiment · no server</div><h1>One common broad-phase format</h1>
<p class="lede">A point moves in a straight line past a fixed triangle. The broad phase sees only conservative bounds and stable IDs. A precise point-to-triangle distance check tells us which candidates are actually within the caller’s margin.</p>
<div class="contract"><strong>Shared record</strong><br><code>PrimitiveBounds { id, kind, min[3], max[3] } &nbsp; → &nbsp; candidate pair [stableIdA, stableIdB]</code><br><span class="note">The caller supplies δ = 0.12 for this experiment. Each primitive gets δ/2 padding. This retains all pairs within Euclidean distance δ and may retain more. The broad phase does not choose the margin.</span></div>
<div class="stages" role="group" aria-label="Experiment stage"><button data-stage="geometry" aria-pressed="true">Geometry</button><button data-stage="bounds" aria-pressed="false">Bounds</button><button data-stage="candidate" aria-pressed="false">Candidate</button></div>
<section class="panel"><h2 id="stageTitle">Geometry</h2><p class="stage-copy" id="stageCopy"></p><svg class="viz" id="scene" viewBox="0 0 800 410" role="img" aria-label="XY projection of a moving point and fixed triangle"></svg>
<div class="legend"><span><i class="swatch sw-blue"></i>conservative bounds</span><span><i class="swatch sw-amber"></i>broad-phase candidate</span><span><i class="swatch sw-green"></i>verified distance ≤ δ</span></div>
<div class="controls"><button class="play" id="play">Play</button><input id="time" type="range" min="0" max="80" value="0" aria-label="Motion timeline"><output id="timeText">0 / 80</output></div>
<div class="readout"><div class="metric"><span>Exact point ↔ triangle distance</span><strong id="distance">—</strong></div><div class="metric"><span>Candidate [1, 2] at this position</span><strong class="status" id="candidateState">—</strong></div><div class="metric"><span>Gap above triangle plane</span><strong id="gap">—</strong></div></div>
<p class="note" id="candidateNote"></p><button class="precise" id="precise" aria-pressed="false">Show precise check</button><p class="note hidden" id="preciseNote"></p></section>
<section class="panel"><h2>One motion interval</h2><p class="note">The endpoint union bounds the entire linear point path. SAP runs on that same <code>PrimitiveBounds</code> record format and returns stable pair <code>[1, 2]</code>. The experiment data and exact distances below were computed by C++ when this HTML was generated.</p><div id="summary"></div></section>
<footer>One triangle and one linearly moving point. Generated C++ results are embedded in this file; JavaScript only replays the states and draws the diagram. No solver integration or method adapter is represented.</footer>
</main><script>
const DATA=__DATA__;
const svg=document.getElementById('scene'), range=document.getElementById('time');let stage='candidate', index=64, playing=false, timer=null, precise=false;
const tri=DATA.triangle, s=DATA.samples;
const sx=x=>100+(x+1.15)*290, sy=y=>330-(y+.5)*210;
function el(name,attrs={},parent=svg){const e=document.createElementNS('http://www.w3.org/2000/svg',name);for(const [k,v] of Object.entries(attrs))e.setAttribute(k,v);parent.appendChild(e);return e}
function rectBounds(min,max,color,dash=''){const x=sx(min[0]), y=sy(max[1]), w=sx(max[0])-x,h=sy(min[1])-y;el('rect',{x,y,width:w,height:h,fill:color==='blue'?'none':color==='amber'?'#fff1d8':'#e2f4eb', 'fill-opacity':'.48',stroke:color==='blue'?'#2667d8':color==='amber'?'#d78300':'#18834b','stroke-width':color==='blue'?2:2,'stroke-dasharray':dash})}
function draw(){svg.replaceChildren();el('rect',{x:0,y:0,width:800,height:410,fill:'#fbfcfe',stroke:'none'});el('line',{x1:sx(-1.12),y1:sy(0),x2:sx(1.12),y2:sy(0),stroke:'#aab4c0','stroke-width':1});el('text',{x:sx(1.04),y:sy(0)-7,fill:'#596575','font-size':12}).textContent='x';
el('polygon',{points:tri.map(v=>`${sx(v[0])},${sy(v[1])}`).join(' '),fill:'#edf1f5',stroke:'#15191f','stroke-width':2});tri.forEach((v,i)=>el('text',{x:sx(v[0])+7,y:sy(v[1])-7,fill:'#15191f','font-size':12}).textContent=`T${i+1}`);
const q=s[index], triangleBounds=DATA.bounds[1], pointRecord={min:q.point.map(x=>x-DATA.delta/2),max:q.point.map(x=>x+DATA.delta/2)};
if(stage==='bounds'){const swept=DATA.bounds[0];rectBounds(swept.min,swept.max,'blue');rectBounds(triangleBounds.min,triangleBounds.max,'blue');}
if(stage==='candidate'){rectBounds(triangleBounds.min,triangleBounds.max,'blue');rectBounds(pointRecord.min,pointRecord.max,q.candidate?'amber':'blue');}
if(stage==='geometry'){const pathY=sy(q.point[1]);el('line',{x1:sx(-1),y1:pathY,x2:sx(1),y2:pathY,stroke:'#667386','stroke-dasharray':'5 5'});}
const isNear=q.withinDelta;el('circle',{cx:sx(q.point[0]),cy:sy(q.point[1]),r:9,fill:isNear?'#e2f4eb':'#fff',stroke:isNear?'#18834b':q.candidate?'#d78300':'#2667d8','stroke-width':3});
el('text',{x:sx(q.point[0])+13,y:sy(q.point[1])+5,fill:'#15191f','font-size':13,'font-weight':650}).textContent='point 1';
document.getElementById('distance').textContent=q.distance.toFixed(4);document.getElementById('gap').textContent=`${q.point[2].toFixed(3)} (point above z = 0 plane)`;document.getElementById('candidateState').textContent=q.candidate?'RETAINED':'REJECTED';document.getElementById('candidateState').className='status '+(q.withinDelta?'yes':q.candidate?'no':'');document.getElementById('timeText').textContent=`${index} / ${s.length-1} · t=${q.t.toFixed(2)}`;
document.getElementById('candidateNote').textContent=q.candidate?(q.withinDelta?'The padded AABBs overlap, and exact distance is within δ.':'False positive: padded AABBs overlap, but exact distance exceeds δ.'):'No padded AABB overlap, so this pair is pruned.';
document.getElementById('preciseNote').textContent=precise?`Precise check: distance ${q.distance.toFixed(4)} ${q.withinDelta?'≤':'>'} δ (${DATA.delta.toFixed(2)}). ${q.withinDelta?'Keep the pair for the solver’s later contact handling.':'The broad-phase candidate, if present, is a false positive.'}`:'';
document.getElementById('preciseNote').classList.toggle('hidden',!precise);range.value=index;}
const copy={geometry:'The dark outlined triangle is fixed. The point follows a straight 3D path; this panel shows XY and reports its height above the triangle.',bounds:'Union of start/end point positions is conservative for linear translation. Blue boxes include δ/2 padding for each primitive.',candidate:'At this timeline position, the same two padded AABBs either overlap (candidate) or do not. Exact triangle distance filters false positives.'};
function setStage(v){stage=v;document.querySelectorAll('[data-stage]').forEach(b=>b.setAttribute('aria-pressed',b.dataset.stage===v));document.getElementById('stageTitle').textContent=v[0].toUpperCase()+v.slice(1);document.getElementById('stageCopy').textContent=copy[v];draw()}
document.querySelectorAll('[data-stage]').forEach(b=>b.onclick=()=>setStage(b.dataset.stage));range.oninput=()=>{index=+range.value;draw()};document.getElementById('precise').onclick=e=>{precise=!precise;e.currentTarget.setAttribute('aria-pressed',precise);e.currentTarget.textContent=precise?'Hide precise check':'Show precise check';draw()};
document.getElementById('play').onclick=()=>{playing=!playing;document.getElementById('play').textContent=playing?'Pause':'Play';if(playing){timer=setInterval(()=>{index=(index+1)%s.length;draw()},85)}else clearInterval(timer)};
document.getElementById('summary').innerHTML=`<div class="readout"><div class="metric"><span>Linear motion samples</span><strong>${DATA.counts.samples}</strong></div><div class="metric"><span>Samples within δ</span><strong>${DATA.counts.required}</strong></div><div class="metric"><span>Broad-phase false positives</span><strong>${DATA.counts.falsePositives}</strong></div></div><p class="note">Full-motion interval candidate pair: <strong>[1, 2]</strong>. At the selected timeline position, the upper readout reports the instant candidate and its exact distance separately. Of ${DATA.counts.samples} instant samples, ${DATA.counts.required} need to reach precise handling; ${DATA.counts.falsePositives} extra instant candidates are safely rejected by exact distance.</p><p class="note">Built-in correctness check: SAP matched the exhaustive AABB oracle in all ${DATA.validation.generatedScenes} deterministic scenes (${DATA.validation.boundsPerScene} bounds each; ${DATA.validation.generatedPairs} candidate pairs checked). The point-to-triangle check retained all ${DATA.counts.required} samples within δ.</p>`;
setStage('candidate');draw();
</script></body></html>)HTML";
}
} // namespace

int main(int argc, char** argv) {
    try {
        std::filesystem::path output = argc > 1 ? argv[1] : "output/Common_Broad_Phase_Demo.html";
        const auto validation = validateGeneratedScenes();
        validateDistanceWitnesses();
        const auto data = makeData(validation);
        std::filesystem::create_directories(output.parent_path().empty() ? "." : output.parent_path());
        auto page = html();
        const std::string token = "__DATA__";
        const auto at = page.find(token);
        if (at == std::string::npos) throw std::runtime_error("HTML data insertion point missing");
        page.replace(at, token.size(), data);
        std::ofstream file(output, std::ios::binary);
        if (!file) throw std::runtime_error("cannot create output file: " + output.string());
        file << page;
        if (!file) throw std::runtime_error("failed while writing output file: " + output.string());
        std::cout << "Wrote offline demo: " << output.string() << " (" << page.size() << " bytes)\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "common_bounds_demo: " << e.what() << '\n';
        return 1;
    }
}
