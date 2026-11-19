# Pre-cleanup evidence and reachability ledger — 19 November 2026

## Immutable root

Original protected source branch HEAD BEFORE cleanup:
0809dea20d6b186f68fcb1acac4ab7200984e24f

Dedicated remote archive branch:
archive/pre-cleanup-2026-11-19
(points at the original commit above; contains all original Git history).

Requested annotated Git tag:
archive/pre-cleanup-2026-11-19
**CREATED AND VERIFIED** on 19 November 2026 by the researcher,
then independently checked against GitHub's annotated-tag object.
The annotated tag object SHA is
`e7b7687b8501bc43d24656bfa255c3ef97be8cc4` and its
peeled object is the protected original commit
`0809dea20d6b186f68fcb1acac4ab7200984e24f`.
The original archive branch still references the same commit.
The tag establishes historical reachability; external release/Zenodo
archival of large evidence is a **separate unfinished gate**.

Created separately:
- cleanup/prune: reachability and ignore-only changes, no removals.
- research/reach-budget-calculus: NEW independent algorithm/proof branch.
- original research/kyoto-decision-coverage: untouched.

NO git filter-repo, history rewrite, force push or destructive
cleanup has been performed.

## Whole-tree inventory and bytes — ACTUALLY VERIFIED

Original protected source commit, including ALL 394 tree entries:
371 tracked files and 23 directory entries. Tracked file bytes:
20,577,173. Of this, 118 original tracked files under output/
occupied 18,214,739 bytes.

The audit branch, after adding the audit script and CI workflow,
contained 373 tracked files (plus its later ignore-only changes).
The audited CI HEAD was
8ef22ae7c92d7e0754576fbf2d63f01ea58ee0aa.
The full audit's SHA-256 ordered-file inventory root digest was
3863a7c1534cba5aec58846f0616401330b9255531b9ccb233aaa404544f6c01.

REVIEW classifications (NOT approval to delete) from full audit:
- KEEP_PROTECTED: 68.
- KEEP_REFERENCED_RUNTIME_BUILD_OR_WORKFLOW: 129.
- REVIEW_HISTORICAL_REFERENCE_BEFORE_MOVE: 28.
- MANUAL_REACHABILITY_REVIEW_NOT_SAFE_TO_DELETE: 30.
- ARCHIVE_EXTERNALLY_WITH_RECEIPT_BEFORE_PRUNE: 118.

Exact full 373-file tracked inventory, all reference analyses,
per-file SHA-256 receipts and the original script's console output
are preserved in the downloadable GitHub Actions audit artifact:
https://github.com/swayam8624/kyoto-common-contact/actions/runs/44531035578/artifacts/14533566686

Artifact ZIP SHA-256:
d54cda562ef3300b86d005847b606fd063dc8661c45ddef6ce071b32aad77345

The review artifact contains:
- GIT_LS_FILES_HEAD_400.txt (full list, because fewer than 400 files).
- GIT_LS_FILES_FULL.txt.
- original-audit/REPO_AUDIT.json and .md.
- ORIGINAL_AUDIT_STDOUT.txt.
- reachability/FULL_REACHABILITY_REVIEW.json/.md.
- reachability/FULL_TRACKED_SHA256SUMS.txt (EVERY file).
- reachability/FULL_TRACKED_FILES.txt.

The original audit printed:
"373 tracked files, 20.59 MB, 0 generated/oversized flags".
This is because its existing rules check absent presentation caches
and >8 MB individual files; it does NOT mean tracked output/
images/archives do not exist. The new full audit explicitly
identifies all 118 tracked output files and their SHA receipts.

## Specific disposition decisions, pending review

- NO .presentation-3d, .presentation-native, .presentation-rebuild,
  .presentation-refine-2026-11-02, .presentation-research,
  .presentation-review-2026-11-02 or .render3d tracked directories
  were found at this commit. These are already absent from the
  CURRENT tracked tree; check original historic branches separately.
- .native-tools/prepare_evidence.py IS tracked source utility code,
  not an automatically removable cache. No workflow/static
  reference was found; human review required.
- All six large historic algorithm families (V5/V6 portfolio,
  Phase14 pointer grids, learned model code, Phase11-16 CPU/GPU
  pressure and dual-probe comparison) still have build targets,
  includes, CI references or research documents. Do not move
  based on a "newer method exists" argument.
- All existing tests and historic negative controls remain.
- Three demo launchers call different entry points:
  open_multicloth_demo.sh wraps open_3d_demo.sh,
  while run_demo.sh starts a Node viewer/server. Therefore
  superficially similar names are NOT evidence of duplication.
- DEMO_RUNBOOK.md describes actual coupled native physics;
  OFFLINE_DEMO.md describes the standalone static HTML scene.
  Those are distinct products, not automatically duplicative.
- Tracked .pptx, .blend, .mp4, .png, .csv, .json and other
  output/ data are historical research evidence and
  reproducibility inputs. DO NOT delete them before verified
  external archival + manifest in Git.
- docs/CBP_PHASE*.md and especially failure reports MUST
  remain available through stable archive links after any
  future move. Move only with README/DEVLOG references fixed.

## External binary archive: NOT YET UPLOADED

No GitHub Release or Zenodo publication is asserted here.
The manifest in the linked CI artifact supplies per-file
SHA-256 receipts. BEFORE a future removal commit:

1. Make a distribution ZIP preserving source-relative paths and
   original bytes (do not silently regenerate modified images).
2. Upload verified bytes to an explicit persistent GitHub Release
   or Zenodo record, recording DOI/release URL and exact ZIP SHA.
3. Store the COMPLETE archive manifest in this repository
   alongside that persistent identifier.
4. Download and independently SHA-check an archive copy.
5. Review references and dependency graph; THEN consider a move
   in a separate "move/archive" commit and full native regression.
6. If unavailable or unverified, keep the original files tracked.

## Source and method integrity rules

- NEVER rewrite existing SHAs or retune frozen V4.
- A static grep miss is NOT a deletion authorization.
- Generated blobs may be ignored for FUTURE adds even while
  historical tracked copies remain in the original tree.
- All output/prune steps are deliberate and non-silent.
- One commit per category (archive, ignore, move); rollback is
  git revert of the individual commit, not history surgery.
- Core CI must remain green and all C++ and Python tests
  meaningful. Never delete a failing regression test.
- New Reach–Budget Calculus research belongs only to the new
  research/reach-budget-calculus branch until independently
  reviewed and proven.
