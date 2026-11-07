# PBookOrbit

## Mission and job to be done
Let a PocketBook reader choose any accessible BookOrbit library and resume that choice after restart, without losing downloads or mixing results across libraries. Maintain a small, upstream-reviewable C++/Qt/QML change.

Read README.md, docs/technical.md, CODE-STANDARDS.md, BEADS.md and docs/developer/workflow.md before changing code. Read your role skill under .agents/skills/pbo-*/SKILL.md. CLAUDE.md is a symlink to this file; edit only AGENTS.md.

## Team
Use one cmux workspace named PBookOrbit with separate architect (Claude), coder (Claude), reviewer (Codex), and QA (Codex) sessions. The orchestrator owns scope, integration and acceptance. Use isolated git worktrees and explicit file ownership. Never discard another agent's work. Shared reports go under .work/library-selector/ until sanitised decisions are promoted into docs/developer/decisions/. Review the actual revision; a handoff is complete only after recipient acknowledgement.

## Test-driven development
For each behaviour change, first add a meaningful automated test and observe the expected failure. Record the red command and failure, implement the smallest correction, then run green and refactor. Tests must exercise production logic, not a second implementation. Use synthetic users, libraries and HTTP responses; never real credentials or personal reading data. Cover persistence, stale replies after switching, fallback when access disappears, offline downloads and account/server isolation. Structural checks and model agreement are not behavioural acceptance.

## File placement and privacy
- src/: shipping C++/QML/assets; tests/: durable synthetic automated tests.
- docs/user/: user-facing instructions; docs/developer/: build, test, design and contribution guidance; decisions/ beneath it for accepted decisions.
- .agents/skills/: role workflows and rubrics; scripts/: reusable project tooling.
- .work/<task>/: ignored temporary logs, experiments and review packets. .work/worktrees/: live worktrees; remove only with git worktree remove after checking status and integration.
- .confidential/: ignored private logs, device databases or inventories. Secrets belong only in ~/.config/pbookorbit/ (directory 700, files 600), never the repo, transcript or issue tracker. Tool-managed credentials remain in their own stores.
- build/: ignored generated artifacts. Release binaries/checksums are deliverables, not source commits.
Never delete unique user files. Preserve unfinished evidence with KEEP.md recording owner, reason and safe cleanup condition. Remove task-owned ephemeral output after distilling results. Do not clean someone else's work.

## Issue Tracking

This project uses **bd (beads)** for issue tracking. The store is embedded Dolt under `.beads/`; its state is committed as `.beads/issues.jsonl` — stage that file with every commit. There is no Dolt remote; do not run `bd sync`, `bd dolt push` or `bd dolt pull`.

Run `bd prime` for the current workflow context (run it at session start and after compaction). Quick reference: `bd ready`, `bd create --title="…" --description="…" --type=task --priority=2`, `bd update <id> --claim`, `bd close <id> --reason="…"`.

The export staging rule applies to project/governance commits; product branches must exclude Beads and all team scaffolding. Prefix pbo. All agents share the root store; do not initialise per-worktree stores. See BEADS.md for safe worktree routing. Only the orchestrator closes the delivery epic after review, QA and physical acceptance.

## Definition of done and landing the plane
1. Meet the accepted behaviour contract with red/green evidence and relevant automated tests.
2. Independent reviewer and QA report the exact commit, commands, outcomes and unresolved findings. Use PASS, FAIL or NOT_RUN; no averaging away blocking failures.
3. Build for the documented SDK/ABI. Verify emulator behaviour separately from physical InkPad 4 acceptance. Do not call device acceptance complete from a build alone.
4. Update issues and relevant docs; preserve open risks as issues. Distil and clean task-owned scratch.
5. Review explicit staged paths for credentials, private data, generated files and unrelated changes; run git diff --check and appropriate checks.
6. Commit coherent changes and export/backup Beads within the assigned phase. Only the orchestrator performs an authorized integration/push; peers hand off their commits or reports without pushing. When a push is authorized, push a named branch to origin and verify its remote SHA. Never force push or push upstream without authorization.
7. For an upstream PR, branch from upstream/main and transfer only product code, tests and user/developer documentation needed by maintainers. Do not include our agent scaffolding, Beads database, Entire configuration, personal endpoints or session history. Audit the entire upstream diff.

## Entire
The installed CLI is `entire` (not a file named entire.cli). Run entire agent-help, then entire agent-help <command> for current flags. Owner approved enablement on 2026-11-07. Entire is enabled for Claude and Codex with automatic session uploads disabled. All seven Codex project hooks are trusted, with clean fresh hooks/list readback. Fresh codex exec and claude -p capture/readback passed; earlier running sessions were not retroactively imported. Never publish session logs to this public fork; keep private capture separate. Manual checkpoints only on owner request.

## Capability policy
Project-local capability selection and fresh-runtime verification are tracked as an open setup issue. Until verified, invoke only tools relevant to this C++/Qt project; global installation is not project approval. Do not change global hook or plugin settings. Do not report this prose as enforced exclusions.
