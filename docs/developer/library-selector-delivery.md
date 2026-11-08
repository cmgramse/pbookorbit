# Library selector delivery

Candidate: `58b48a1564b6d6c6a5e3e952bbb12a17be72b514`, published to the fork branch `feature/library-selector`. Remote SHA verified. Upstream baseline: `b063807`. The four product commits contain source, tests and product documentation only; project/governance was not merged into them.

## Software evidence

Independent review approved the exact implementation and ARM artifact with no unresolved actionable product findings. Independent QA passed 22 checks, then independently built and ran the committed client suite (73 passed) and QML suite (22 passed), with zero failures/skips and no QML warnings. Counts include QtTest setup/cleanup rows. Tests use synthetic data, loopback HTTP and isolated databases. Targeted regression tests exposed and verified repairs for recovery after an empty library response and hardware paging behind the fallback modal.

TDD evidence distinguishes the initial runtime-red run from later mutation checks; it does not claim every baseline test failed before implementation. Architecture revision2 deliberately preserves existing Find books behaviour. Pre-existing cross-server storage and authentication-callback issues are separately tracked, not silently declared fixed.

## Build and installation

Built from an immutable archive of the candidate SHA using the published B2886.10 SDK (SHA256 `18405fb276535d53a4b70d2bbf4033313cded58da5002d563ccf7e1838440c0b`), Qt6.8.2 and cross-Clang15.0.3. Upstream pocketbook-kit is unavailable publicly; a local build adapter reproduced baseline executable code and provided the same ARM ABI/dependency set. The final link includes all three new QML/JS resources. No new private Qt import or unsupported Qt/GLIBC requirement was found.

Artifact: `pbookorbit.app`, 3,086,880 bytes, SHA256 `db9918e78d17562e3e8c46d36efda86535b921db73a03af4ed1b3f8277dd0184`.

Installed and checksum-verified on the owner-connected InkPad4 on 2026-11-07 at13:41UTC. Existing executable and credential-bearing settings DB backed up outside the repository under the project secret store. Settings DB was unchanged by installation. Volume safely ejected. Device Qt6.10.3 uses the same QML runtime-compilation fallback as the original Qt6.8.2-built app.

## Physical acceptance

Owner accepted the installed feature on 2026-11-08: “its working great”. This is owner-reported physical success, distinct from independently executed host checks; no additional detailed manual-test matrix was reported.

Rollback: restore the preserved original executable only; do not overwrite the settings DB unless recovery explicitly needs it. No user book files were modified.

Upstream PR submitted: https://github.com/cooperaj/pbookorbit/pull/2 — Add persistent library selection. Readback confirms OPEN, not draft, base main and head contrib/library-selector at e80c8e96f65fd47781f238deeeb459cbb4b4e5e1. The new one-commit contribution removes internal session-link trailers while preserving co-author credit. Only the README device note differs from the tested source; application, tests and build inputs are identical. The original tested feature branch is preserved. Team reports, Beads, runtime configuration and private data remain excluded. Fresh 2026-11-08 QA reran the 73-client/22-QML suites without failures, skips or warnings, and independent contribution review approved the exact PR branch and body.
