# Core multi-cloth safety and invocation audit

Audit scope: `research_multicloth.cpp`, `online_partitioner.cpp`, their public
headers, and `research_multicloth_tests.cpp`. This note records the actual
call path and the bounded fixes made in this audit; it is not a complete
physics or continuous-collision safety proof.

## Invocation matrix

| Mode | Called from `MultiCloth::step` | Candidate path | Physics path |
| --- | --- | --- | --- |
| `RebuildSAP` | Every one of the five constraint iterations | Builds padded current bounds and calls `live::candidates` | Returned pairs go to `projectContacts` |
| `GlobalGuard` | Every constraint iteration | `BroadPhaseService::query` with guarded reuse | Cached pairs are rechecked against current bounds before response |
| `RegionalCertified` | Every constraint iteration | `RegionalCandidates::query`; current padded bounds, local certificates, optional region sweep, exact current-AABB revalidation | Returned pairs go to `projectContacts` |
| `OnlineContactAware` | Same regional query path, then once after the full timestep | `OnlinePartitioner::observe` consumes accumulated query work and actual correction counters; split/merge or traversal changes update next-frame region assignment | It changes region IDs only; the same `projectContacts` implementation is used |

`RegionalCandidates::query` rejects duplicate IDs, non-finite or inverted raw
bounds, invalid margins, and non-finite results after padding or local guard
expansion. Rebuilt envelopes are committed only after every member envelope
is finite. A rejected query does not expose its partial result.

## Bounded changes

- Reject a finite margin and finite raw coordinates when their padded AABB
  overflows to infinity. Such bounds cannot be used as a meaningful cache
  certificate.
- Reject any non-finite region envelope before it can be stored or swept.
- Remove a redundant `unique` pass after final sorting. Each primitive is
  assigned to one region; each region pair is visited once; and each cached
  block is sorted and deduplicated at construction. Thus each candidate pair
  has one source in the output. Sorting remains because callers and audit
  checks consume ordered pairs.

## Verification boundary

The multi-cloth tests compare candidate output to exhaustive current padded
AABB enumeration and compare the physical trajectories with the SAP path.
Those checks establish the tested discrete candidate and solver behavior for
their fixtures. They do not establish continuous-time collision avoidance,
intersection-free cloth, or stability for arbitrary geometry and parameters.
