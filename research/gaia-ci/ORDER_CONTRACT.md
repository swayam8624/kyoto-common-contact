# Gaia order-contract diagnostic (2026-11-28)

Independent Linux run 46161778177 compiled the pinned upstream CPU detector and the frozen selector adapter. Raw traversal order differed on every one of 48 matched frames. Exact per-query contact multisets, flags, minima, counts, and face-link arrays matched. FV ordering differed for 498 queries, EE ordering for 13,091. VF arrays were identical.

The separate gaia_probe_canonical.inc is an optional replacement for the diagnostic probe only. It sorts FV and EE output arrays for both implementations when KYOTO_CANONICAL_ORDER is set; VF ordering and face links remain untouched. It also adds a repeated baseline A/A check. The file was built and executed locally using Clang 17 and the CI-exported dependencies. All six workloads (48 frames) passed byte-identical output and the A/A check. This does not establish strict equivalence to the unmodified original traversal order, universal parity, or end-to-end Gaia solver performance. The original raw probe remains unchanged.

Counts: 5,376 VF, 8,592 FV, 13,896 EE queries; 117,130 contacts, 11,456 face summaries and 7,887 independently checked face-link references.

Embree 3.12.2 from Ubuntu 22.04 was used, alongside its legacy TBB2 runtime and Gaia's oneTBB12. Linker warns of possible runtime conflict. Native Apple Silicon and exact Embree 3.13.1 remain unverified. Timings are exploratory fixed-order query-only measurements, not solver benchmark conclusions.
