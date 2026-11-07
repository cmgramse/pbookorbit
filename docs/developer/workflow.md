# Development and acceptance

## Branches
project/governance holds our local project operating layer. library-selector/coder is the implementation worktree branch based on upstream. Review and QA check the exact implementation commit in their own worktrees. The upstream PR branch receives only the reviewed product commits. Do not merge governance into it.

## Gates
Architecture accepted -> RED test evidence -> minimal implementation -> GREEN/refactor -> independent review -> QA -> ARM build -> physical device acceptance -> upstream PR readiness. Each transition records evidence and SHA in Beads. Review findings must be checked against source before accepting or disputing them.

## Build status
Upstream Makefile offers make setup, make pc, make arm, make run and make run-offline. These are not yet validated in this checkout. QA is resolving the kit submodule and B288 SDK prerequisites. Test commands will be documented when the first meaningful test harness is delivered; do not invent passing checks.

## Cleanup
Keep live worktrees and unresolved review evidence. At task close promote sanitised decisions into docs/developer/decisions, then remove only task-owned disposable files. Never run broad git clean. Inspect git status in each worktree before removing it. KEEP.md records retained evidence and cleanup trigger.

## Entire status
Owner-approved integration enabled for Claude and Codex on 2026-11-07 with --skip-push-sessions and telemetry disabled. Existing Beads startup hook preserved. Fresh hooks/list readback confirms seven trusted project hooks and no errors. Fresh codex exec and claude -p smoke sessions were captured and read back by Entire. Earlier running sessions were not retroactively imported. Never export private history to the public upstream or fork.
