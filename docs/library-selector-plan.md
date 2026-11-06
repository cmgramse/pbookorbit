# Library selector delivery plan

Fork: cmgramse/pbookorbit. Upstream: cooperaj/pbookorbit. Owner requested Claude coding, Codex review, separate QA, and a Claude architect in cmux.

## Scope
Expose accessible libraries and a native-looking e-ink-friendly selector; persist selected library scoped to server/account; deterministic fallback if selection is unavailable. Refresh books, search, authors and series on switch. Audit collection semantics rather than assuming collections belong to one library. Guard stale asynchronous replies. Preserve downloads, offline reading, and existing accounts. One-library accounts should remain simple.

## Evidence and boundaries
Existing source selects libraries.first() in two paths. Live native API test returned two libraries and passed password login, library listing, book query, token refresh and logout. No Cloudflare changes required. Do not access credentials, production databases, user book data or real device storage. Use synthetic fixtures and separate test databases. No upstream PR, release publication or device installation in this phase.

## Sequence and ownership
1. Architect (Claude): source-backed design, edge cases and minimal contract. Write architecture.md and architecture-ready.md in shared task directory.
2. Coder (Claude): wait for architecture-ready.md, then implement in own feature branch. TDD for meaningful switching/state regressions. Own application changes. Commit coherent change; write implementation-ready.md with exact commit and checks.
3. Reviewer (Codex): inspect design now; review exact implementation commit later in own worktree. No production code edits. Write review.md with findings and reviewed SHA.
4. QA (Codex): prepare synthetic multi-library test matrix and validate build tooling/InkPad 4 B288 compatibility now. Test exact implementation commit later in own worktree. Own test evidence only; coordinate proposed test changes with coder. Write qa.md.
5. Orchestrator: reconcile findings, require reruns after fixes; prepare ARM artifact, checksum, rollback instructions. Physical InkPad acceptance remains separate: switch both libraries, search, restart, open download, offline reading.

## Team protocol
You are not alone: never revert others. Each runtime works in its own worktree. Shared reports only in .work/library-selector at the root checkout. Record ACK with runtime and assignment before substantial work. Never claim another runtime's result as your own. If waiting on another role, record waiting state and return to prompt; orchestrator dispatches the next phase. Do not merge/push code without orchestration. Default installed runtime models; report actual model where available. No secrets or private inventories in git. Do not copy ebook-library project instructions into this independent project.
