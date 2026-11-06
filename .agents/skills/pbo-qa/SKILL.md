---
name: pbo-qa
description: Prepare and execute independent PBookOrbit QA against an exact implementation commit, using synthetic fixtures and separate build, emulator, and physical-device evidence. Use for QA handoffs and SDK/build investigation, not application implementation or release publication.
---

# PBookOrbit QA

## Job to be done (JTBD)

When a PocketBook reader chooses another accessible BookOrbit library, they need the app to show that library consistently and remember it after restart, while preserving downloads and offline reading. Give the orchestrator reproducible evidence that the agreed behavior works at the named revision, or a precise failure/dependency that prevents acceptance.

QA owns test evidence and this skill. The coder owns application/test implementation; coordinate proposed durable test changes with that role. The orchestrator owns scope, integration, AGENTS.md, BEADS.md and final acceptance. Do not close another role's work or the delivery epic.

## Inputs

- Current user assignment, phase and restrictions; root AGENTS.md, BEADS.md, CODE-STANDARDS.md and docs/developer/workflow.md.
- Accepted task plan and architecture contract, including readiness marker. For library selection use docs/library-selector-plan.md and shared `.work/library-selector/` reports.
- `implementation-ready.md`: exact commit SHA, branch, changed behavior, runnable test commands, coder red/green evidence and known gaps. Missing marker permits preparation only.
- QA's assigned worktree, shared report directory and routed project issue. Verify git HEAD/status and issue-store location; do not infer identity from the shell directory alone.
- For executable work: pinned kit revision, SDK/ABI and firmware target, isolated emulator/storage and synthetic fixtures. For physical acceptance: separately authorized device session and its actual firmware.

## Outputs

- `qa-ack.md` first: runtime/model if exposed, assignment, worktree, HEAD, phase, ownership and dependencies.
- `qa-plan.md`: requirement-to-case matrix, expected outcomes from the contract, fixtures/isolation, commands, build prerequisites and separate device acceptance checklist.
- `qa.md`: exact tested SHA and environment, rubric result per row, commands/exit codes/counts, evidence paths, reproducible defects and unresolved dependencies with owner/next action.
- Sanitized logs, request traces, DB comparisons, screenshots and artifact hashes only where produced. Report coder evidence explicitly as coder evidence, never independent QA execution.
- Project issue updates through the verified root store. If routing disagrees with BEADS.md, report it to the orchestrator before writing; never initialize another store or change global configuration to compensate.

Keep temporary reports under the assigned ignored task directory. This skill and other agent/Beads governance belong on `project/governance`, never in the upstream product PR. Do not stage, commit, merge or push beyond the active assignment. Preserve unrelated files and other runtimes' work.

## Reuse installed methods, with portable fallback

These methods were inspected when authoring this role. Resolve by skill name in the current harness's available catalogue; read the full installed skill before using it. Never depend on a particular user's cache path or install a plugin merely to execute QA.

| Installed skill | Use here | Self-contained fallback when absent |
|---|---|---|
| `superpowers:verification-before-completion` | Before a test/build/completion claim | Identify the command that proves the claim; run it against the recorded SHA; read its complete output and exit code; state only what it establishes. A dry run is not a build; a build is not runtime acceptance. |
| `superpowers:systematic-debugging` | Investigating a failed case or tooling boundary | Reproduce minimally, record exact error and environment, trace the first failing boundary, compare a working reference, test one explanation at a time. Send a reproducible defect to the coder rather than editing application code. Stop when further diagnosis requires unavailable inputs or unauthorized mutation. |
| `skill-creator` | Maintaining this role skill | Preserve scope and portable inputs; keep frontmatter valid; define observable outcomes and gates; validate formatting and walk through ready, blocked and failing scenarios without claiming those walkthroughs prove product behavior. |

Skill guidance does not authorize installations, downloads, production access, dispatching agents or device changes. Existing user authorization controls each action.

## Workflow

1. **Orient and acknowledge.** Read the assignment and root guidance, inspect only relevant project knowledge where available, verify worktree/HEAD/status and write ACK before substantial work. Route issue reads/writes using BEADS.md and check actual store location before claiming an issue. Preserve a clean baseline and other agents' edits.
2. **Choose phase.** If implementation readiness is absent, prepare the plan and investigate prerequisites only. Check handoff files once again before returning; report dependencies instead of polling indefinitely. If present, read the exact SHA/contract and test only that revision in your assigned worktree using the orchestrator's authorized checkout route. Do not test a moving coder checkout.
3. **Make requirements executable.** Map every accepted requirement to a case and explicit expected result. Cover zero/one/many libraries, fallback, server/account persistence, list/search/author/series refresh, collections, stale replies, downloads/offline, migration, errors and UI. Unresolved behavior is a contract dependency, not an opportunity to invent an assertion.
4. **Inspect build prerequisites.** Read Makefile, project file, `.gitmodules` and pinned kit metadata. Inspect runner/compiler availability, container daemon architecture, SDK selection and cached dependencies before execution. A reachable daemon is not a usable SDK. Check upstream source/docs for the exact target; do not substitute B300 for B288. Under preparation-only authorization, do not run setup, pull images, initialize/download SDKs or install packages.
5. **Isolate before execution.** Use loopback fake APIs, synthetic accounts/library IDs and fresh DBs via `PBOOKORBIT_DB`. Isolate the whole emulator filesystem as well: cache, downloads, logs and launcher/native calls can escape database-only isolation. Never use credentials, production DBs, user reading data or real device storage as fixtures. Bind mock servers locally and stop task-owned processes after use.
6. **Run independent checks.** Execute relevant production-logic tests and integration/UI cases, record each result and exit code. Control delayed replies deterministically; repeat opposite order and rapid switching. Compare persistence after process restart and file/DB records after download/offline cases. Verify regression evidence without reverting the coder's checkout. Structural checks are supplementary, not functional passes.
7. **Separate platform gates.** Record host tests, emulator interaction, B288 ARM compilation/ELF dependencies and physical device acceptance separately. Inspect target SDK/Qt compatibility, including QML bytecode behavior. Do not call physical acceptance complete from emulator screenshots or artifact creation. Device installation remains outside this role unless explicitly assigned.
8. **Report and hand off.** Apply every rubric row below; provide defects with steps, expected/actual results, SHA and evidence. Retest affected cases after fixes and identify unchanged evidence by SHA. Update the verified issue without closing the delivery epic. Preserve unfinished evidence and remove only task-owned disposable output. Return with a bounded dependency list and next dispatch trigger.

## Complete measurable rubric

Use `PASS`, `FAIL`, or `NOT_RUN` for every row. Add `BLOCKED: dependency / owner / next read or action` to NOT_RUN rows where appropriate. N/A is allowed only for a requirement explicitly outside the accepted phase/scope, with a written reason; it cannot erase a delivery gate. No weighted average or percentage can override a failing gate.

| Gate | Measurement and PASS threshold | Required evidence |
|---|---|---|
| R1 Revision/ownership | Every execution names one exact SHA; 0 edits outside owned scope; no moving checkout used | HEAD/status, test revision and scoped diff |
| R2 Coverage/contract | 100% of accepted requirements map to at least one case with expected outcome; 0 unresolved assertions for executed cases | Traceability matrix and contract reference |
| R3 Discovery/selection | Zero, one and multiple-library cases pass; selected label and all scoped request IDs agree; 0 inaccessible/invalid-library requests | Mock traces and UI/state assertions |
| R4 Persistence/isolation | Restart, token refresh, logout/login, two accounts on one server and same account name on two servers pass; 0 selection leakage with overlapping library IDs | Isolated DB before/after plus requests |
| R5 Fallback/errors | Removed/forbidden/reordered/empty library cases and malformed/network/server errors match exact fallback/error contract; 0 crashes, loops or false success | Expected/actual assertions and bounded request count |
| R6 Refresh/navigation | Books, search, authors, series, counts, jump/paging and nested views agree with active context after A→B→A; 0 stale cache rows presented as current | Model assertions and request sequence |
| R7 Async safety | Late success AND error for scoped requests, delayed discovery, rapid A→B→A and logout/account change pass in controlled reverse order; 0 stale UI/cache/session writes | Deterministic response schedule and assertions |
| R8 Collections/BookFinder | Global/mixed collection and scan-during-switch cases match architect's audited contract; 0 incorrect filtering, membership loss or cross-context matching | Synthetic membership/files and result assertions |
| R9 Data/offline regression | Synthetic legacy DB, completed/in-flight downloads, offline restart/open and reconnect pass; 0 unintended file/hash/mapping losses | DB comparisons, synthetic file hashes, native-open stub trace |
| R10 UI/e-ink | Every planned target layout and interaction case passes: selection, long/Unicode/duplicate labels, scroll/dismiss/back, grayscale/night mode and supported controls; 0 clipped required controls or new idle redraw loop | Screenshots plus interaction/event observations; measured loop check |
| R11 Test integrity | All mandatory automated cases pass with 0 failures; behavior tests call production logic; meaningful change has observed red/green evidence | Commands, counts, exit codes; separately attributed coder red/green |
| R12 Build/ABI | Exact SHA compiles successfully for documented B288 SDK; artifact exists; ELF machine/loader/dependencies match target; checksum recorded | Full build log, SDK revision, binary inspection, hash |
| R13 Emulator runtime | Exact build launches in isolated target profile; all required emulator scenarios pass; 0 unexplained fatal startup/QML errors | Startup log including Qt versions and scenario evidence |
| R14 Physical acceptance | Authorized InkPad 4 session passes both-library switch, search, restart, downloaded-book open and offline reading; all 6 observations recorded with firmware/build | Owner/device evidence; NOT_RUN until that separate phase |
| R15 Safety/upstream boundary | 0 credentials/private data/real storage fixtures; 0 unapproved downloads/device writes; 0 governance files in proposed upstream diff | Isolation setup, action log and complete upstream diff audit when supplied |
| R16 Handoff quality | Every planned case has status; every defect/dependency has owner and next action; 0 unsupported pass claims; affected failures retested at fix SHA | qa.md, evidence links and routed issue update |

For R7 include the response families the implementation actually issues: books, search, authors, series, details, jump buckets, pagination, library discovery and relevant collection/scan callbacks. A missing family needs a source-backed reason, not omission.

## Definition of done (DoD)

**Preparation done:** ACK and matrix exist; requirement coverage and safe fixture design are checked; local build prerequisites are inspected; each missing input/tool has a named owner and next action. Functional/build/device rows remain NOT_RUN. This completes preparation only.

**Independent software QA done:** Exact implementation SHA is recorded; R1–R11, R13 and R16 pass for the accepted software scope; R15 safety checks pass and any pending upstream diff audit is explicitly separated. No mandatory case is skipped, no blocking defect remains, and all fixes have relevant independent reruns. If the build is assigned to a later orchestrator phase, R12 stays NOT_RUN and the report states that boundary.

**Delivery acceptance done:** All R1–R16 gates pass, including exact-target build, full upstream diff boundary and separately obtained physical evidence. Only the orchestrator closes delivery. A blocked preparation or software phase returns a useful report; it never silently lowers the delivery DoD.
