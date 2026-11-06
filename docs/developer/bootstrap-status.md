# Bootstrap verification

Verified on 2026-11-06 on the maintainer Mac:

- AGENTS.md is shared through a CLAUDE.md symlink.
- Five local role skills define jobs, inputs, ownership, evidence rubrics and definitions of done. scripts/team-session.sh generates each runtime seed; all five --print invocations and Bash syntax check passed.
- Beads uses the root embedded Dolt store, pbo prefix, maintainer routing, export.auto=true and no Dolt remote. Dependency chain links architecture, implementation, review/QA and device/PR preparation. Native local backup synced successfully. JSONL is an issue export, not a complete backup.
- Ignore checks passed for scratch, private data, generated build output, environment files, embedded DB and machine-local backup settings.
- Project Codex configuration parses; fresh `codex mcp list --json` readback confirms unrelated MCP servers disabled and selected project/documentation tools enabled. Full personal-skill catalogue and all runtime entry points remain OPEN in pbo-2ee. Already-running runtimes predate this selection.
- Entire CLI is installed but NOT enabled. pbo-bax tracks explicit enable confirmation, private session handling, hooks and fresh-runtime capture verification.
- Product tests, emulator, ARM build and physical-device acceptance are NOT_RUN at bootstrap. QA identified unresolved kit access and SDK/ABI verification; these cannot be replaced by governance checks.

The governance branch is excluded from upstream feature delivery. Review/QA reports live in the task scratch directory until their findings are distilled; unfinished evidence is retained with KEEP.md.

Independent Codex governance review resolved pbo-scf (phase-specific push authority). Independent Codex QA resolved pbo-sdy after reproducing a false-positive boundary check and verifying its replacement against allowed product paths, forbidden governance paths, missing base and simulated Git failure. Orchestrator additionally verified empty candidate rejection. Product acceptance remains separate.
