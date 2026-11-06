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
CLI installed; integration not enabled yet. Enable only after the separately required owner confirmation. Use --skip-push-sessions; inspect effective settings and hook chaining before claiming acceptance. Never export private history to the public upstream or fork.
