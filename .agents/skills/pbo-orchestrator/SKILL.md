---
name: pbo-orchestrator
description: Coordinate an authorized PBookOrbit delivery across architect, coder, reviewer and QA, reconciling exact-revision evidence and keeping governance separate from upstream changes. Use for orchestration assignments, not bounded peer tasks.
---

# PBookOrbit orchestrator

## Job to be done

Turn an accepted user outcome into independently verified delivery without conflicting edits, lost dependencies, or claims stronger than the evidence. Preserve a small upstream product diff and report physical acceptance separately from tests and builds.

## Inputs and ownership

Read root `AGENTS.md`, `BEADS.md`, `CODE-STANDARDS.md`, `docs/developer/workflow.md`, the task plan and existing reports. Establish accepted scope/non-goals, owner authorization, role assignments, owned paths, worktrees, dependencies, acceptance criteria and report directory. For the current task use `.work/library-selector/` and `docs/library-selector-plan.md`.

Own coordination, acceptance reconciliation and authorized integration. Architect owns the contract, coder application changes, reviewer independent findings and QA independent test evidence. Do not rewrite another role's report or self-certify its work. Use the user's selected runtimes and default installed models; record actual identity only when exposed. Skill use grants no new permission to publish, push, install on a device or alter credentials/settings.

## Inspected reuse and portable fallback

These sources were inspected at authoring (2026-11-06); read the installed version before invoking it.

- `~/.agents/skills/rp-orchestrate-cli/SKILL.md`: reuse bounded work items, explicit ownership/dependencies, self-contained briefs and verify-before-next-dispatch. Use the existing PBookOrbit cmux roles rather than creating duplicate agents. RepoPrompt is optional: if actually used, first run `rpce-cli --version`, then `rpce-cli -e 'windows'`, verify the target root/workspace and use the selected window explicitly. CLI only; never register it as MCP. This task's bounded-wait protocol takes precedence over the reference's long-wait/polling guidance.
- `~/.codex/skills/beads/SKILL.md`: reuse issues with outcomes, dependencies and evidence. Root `BEADS.md` determines routing/export placement; verify current help before commands. Do not copy generic init, remote-sync or every-commit export advice into product branches.
- [pbo-reviewer](../pbo-reviewer/SKILL.md): use its review evidence/decision contract when consuming a reviewer packet, not as a replacement for independent review.

Without these external skills or tools, coordinate using the accepted plan, Git and shared reports. Produce dispatch briefs containing goal, owned paths, dependencies, checks, output path and stop condition. If no authorized agent transport exists, publish the brief and report that dispatch/receipt is pending; do not claim an agent has received it. Do not install tools or change global configuration to satisfy this workflow.

## Workflow

1. Verify the main checkout, branches, worktrees and dirty state. Read existing ACKs before dispatching. Keep `project/governance` in the root checkout and use the assigned product worktrees. Record an orchestration ACK before substantial work. Do not move live worktrees.
2. Translate the accepted scope into measurable acceptance checks and a dependency graph. Use the verified root Beads store for durable issue state; check `where` and list/show routing. A contributor-routing warning is a blocker to tracker writes even if `where` prints the expected directory. Record that dependency in the authorized report and return it to the configuration owner; do not initialize another store or silently change roles.
3. Dispatch only within the authorized team workflow. Each brief names current phase, worktree, owned files, sibling boundaries, input report, expected output and stopping condition. Require a recipient ACK; transport acknowledgement is not receipt. Do not wait indefinitely for it: report the pending recipient and next dispatch action, then return to prompt.
4. Obtain source-backed `architecture.md` plus `architecture-ready.md`; reconcile reviewer design findings before coder implementation. Allow QA prerequisite inspection and independent design review to proceed before implementation. Missing handoffs do not block unrelated authorized work.
5. Require the coder's `implementation-ready.md` to identify an immutable SHA, changed paths, meaningful RED/GREEN commands/results and limitations. Verify the commit exists. Dispatch reviewer and QA against that exact revision in separate worktrees. A model's agreement or a static grep does not demonstrate behavior.
6. Reconcile each acceptance criterion against source review, automated checks, build, emulator and device evidence separately. Track blocking findings by ID, owner and fix SHA. Any fix creates a new review target: rerun affected checks and reconcile prior evidence before advancing. Never combine PASS results from different revisions as one passing delivery without explicit justification and revalidation.
7. For the current library-selector contract require persistence/account isolation, deterministic fallback, stale reply/error rejection, collection semantics, usable selector and preserved downloaded/offline reading. Prepare ARM artifact, SHA-256 checksum and rollback instructions only when build prerequisites are verified. Physical InkPad 4 B288 acceptance requires its own recorded switch/search/restart/download/offline outcomes and authorization for device actions.
8. Prepare upstream scope only within current authority. Branch from a verified `upstream/main` revision and transfer reviewed product commits only; never merge `project/governance`. Inspect commit contents before transfer and audit the complete resulting diff and commit range afterward. If commits mix governance and product, return to the coder for a clean split instead of transferring them blindly.
9. Keep `.agents/`, local agent configuration, Beads, Entire/session history, private endpoints and operational reports out of the upstream PR. Only maintainer-relevant user/developer documentation belongs there. An ignored directory is not proof that no tracked file leaked: inspect both staged paths and `git diff --name-status <upstream-base>...<candidate>`, then read the full diff. Record both SHAs and excluded categories.
10. Publish a phase rollup with exact revisions, artifact/report links, evidence owners, unresolved issues and next actions. Export Beads only to governance after verified routing; review explicit staged paths. Commit/push/PR/device actions follow current authorization, never generic landing instructions. Verify remote SHA only after an authorized push. Cleanup only owned disposable artifacts; retain unresolved evidence and live worktrees. Close the delivery epic only after all required acceptance gates, including physical acceptance, actually pass.

## Measurable completion rubric

Use PASS, FAIL or NOT_RUN for each applicable gate, with evidence. N/A needs an accepted scope reason. Missing prerequisites keep that gate open; there is no average score.

| Criterion | PASS threshold |
| --- | --- |
| Scope and ownership | Every acceptance criterion has an owner/check; every work item has exclusive file ownership, dependencies and a stop condition |
| Dispatch | Every claimed handoff has a recipient ACK; missing ACKs are explicitly pending |
| Revision integrity | Implementation, review, QA and deliverable identify exact SHAs; all post-review fixes have affected-check reruns |
| Behavior evidence | All required behavior tests have meaningful RED/GREEN evidence against production logic; no blocking failure or unowned gap |
| Independent decisions | Review and QA reports are present and separately attributed; all blocking findings resolved or delivery remains open |
| Build and device | Target SDK/ABI, artifact checksum and rollback documented; emulator and physical B288 results recorded separately |
| Upstream isolation | Full candidate diff and commit inventory audited against recorded upstream base; zero governance/private/session artifacts transferred |
| Closure | Issues/evidence reconciled through verified routing; final rollup read back; cleanup preserves others' work; no unauthorized external action |

## Definition of done

A phase is complete when its owned outputs satisfy the applicable rubric and the next role has acknowledged receipt, or the rollup explicitly states the missing receipt/dependency and returns control without claiming closure. Delivery is complete only when every required gate passes on the delivered revision and the epic has been reconciled. PR readiness is not publication; a build is not device acceptance; governance completion is not product completion. Pending prerequisites must name the owner and exact evidence needed to resume.
