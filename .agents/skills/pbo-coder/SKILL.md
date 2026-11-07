---
name: pbo-coder
description: Implement an accepted PBookOrbit architecture contract test-first in the coder worktree, producing one reviewable commit range ready for an upstream PR, with red/green evidence. Use for implementation handoffs and fixes after review or QA, not for changing the contract, reviewing your own work or publishing.
---

# PBookOrbit coder

## Job to be done (JTBD)

When the architecture contract is ready, a PocketBook reader needs the behaviour to work on their device without losing downloads, offline reading or their account, and the upstream maintainer needs a small change they can review and merge. Give the reviewer and QA an exact commit that does what the contract says, with evidence they can rerun, and nothing else in the diff.

The coder owns application code, durable tests under `tests/`, the user and developer docs the change affects, and this skill. The architect owns the contract. The reviewer and QA own their verdicts. The orchestrator owns integration, pushes, the upstream PR and acceptance.

## Inputs

- The current assignment. A newer owner or orchestrator message overrides an older one.
- Root `AGENTS.md`, `BEADS.md`, `CODE-STANDARDS.md` and `docs/developer/workflow.md`. These live in the main checkout and are not in the coder worktree: read them by absolute path. `CODE-STANDARDS.md` is a stub, so existing code conventions win where they differ.
- `.work/<task>/architecture-ready.md` and `architecture.md` at the contract revision named there. Without the marker, read and report only: write no implementation.
- `design-review.md` and `qa-plan.md`, for what will be checked. Later, `review.md` and `qa.md`.
- The assigned worktree and branch, with HEAD and `git status` verified. For the library selector this is `.work/worktrees/coder` on `library-selector/coder`, based on upstream.
- Synthetic fixtures only. Never real credentials, production databases, personal reading data or device storage.

## Outputs

- `coder-ack.md`, written first: runtime and model, assignment, worktree, branch, base SHA, contract revision.
- Commits on the feature branch: coherent, each building, in the repository's message style (a plain sentence saying what changed).
- `implementation-ready.md`, written last, with:
  - the exact SHA, branch and base;
  - the contract revision implemented, and any deviation with its reason;
  - changed files, and the boundary check output;
  - a red/green table: test, red command and failure, green command and result;
  - the full verification commands with exit codes and counts;
  - `NOT_RUN` items, each with its dependency and owner;
  - known gaps.
- A Beads update through `bd -C /Users/claudius/Developer/pbookorbit`, with the SHA and evidence, on the implementation issue only.

## Reuse installed methods, with portable fallback

These methods were inspected when this role was written. Resolve them by name in the current harness and read the full skill before use. Do not depend on a cache path. Where a skill's default conflicts with this table, this table wins.

| Installed skill | Use here | Override for this project | Self-contained fallback |
|---|---|---|---|
| `superpowers:test-driven-development` | Every behaviour change | Record each red in the evidence table. If the toolchain is missing, stop and report: never write production code first on the promise of a later test | Write one behaviour test, run it and see it fail for the expected reason, write the least code to pass, rerun, then refactor while green |
| `superpowers:systematic-debugging` | A failing test or surprising behaviour | Change one thing at a time. A flaky async test is a bug, not a retry | Reproduce, read the full error, trace to the first wrong value, test one explanation, fix the cause |
| `superpowers:verification-before-completion` | Before any claim of passing or done | None | Run the full command fresh, read its exit code and counts, state only what they show |
| `superpowers:receiving-code-review` | Handling `review.md` and `qa.md` | Check each finding against the source before accepting or disputing it. Dispute with evidence in the report | Restate the finding, check it against the code, reproduce it with a failing test where it is behavioural, fix it, and rerun |
| `superpowers:writing-plans` | Only if the contract needs breaking into steps | Save to `.work/<task>/`. Its default `docs/superpowers/plans` would enter the branch | A short ordered list of test-sized steps in the scratch directory |
| `superpowers:requesting-code-review` | Do not use for sign-off | The Codex reviewer owns review, at the exact SHA. A self-spawned reviewer is not review evidence | Write `implementation-ready.md` and hand off |
| `superpowers:finishing-a-development-branch` | Do not use its merge, push, PR or discard menu | No merge, push or PR without orchestration. The worktree stays | Stop at the commit and hand off |
| `superpowers:brainstorming`, `superpowers:using-git-worktrees` | Do not use | The contract and the worktree already exist | Not needed |

## Process

1. **Acknowledge.** Read the assignment and the root guidance, verify HEAD and status, write the ACK. Claim the issue through `bd -C`, after `bd where` confirms the root store.
2. **Gate on the contract.** Read `architecture-ready.md`. If it is absent, write the state to `coder-readiness.md` and return to the prompt. Do not poll or infer a design.
3. **Get a test route running first.** Follow the contract's test design. Build the harness and prove it can fail before touching production logic. Tests call production code, never a copy of it. If no toolchain can run it, stop and report the dependency with the exact command and error.
4. **Red, green, refactor, per obligation.** Take the contract's test list in order. For each: write the test, watch the expected failure, record it, make the smallest change, rerun everything so far. Fake HTTP binds to loopback and releases replies in a controlled order. Databases are temporary files.
5. **Stay in scope.** Implement the contract. If it is wrong or silent, ask the architect through the shared report and record the answer: do not decide alone and do not widen the change. Bring code up to standard only where this change touches it. No reformatting, no opportunistic fixes.
6. **Update the docs in the same change.** Update what the behaviour made false, such as `docs/technical.md`, `README.md` and `TODO.md`.
7. **Check the boundary.** Run the check below after committing, and read the full staged diff before committing. A non-zero exit stops the handoff. Stage explicit paths only, never `git add -A` or `git add .`. Run `git diff --check`.
8. **Commit.** Commit coherent changes on the feature branch. Do not stage `.beads/issues.jsonl` here: `BEADS.md` and `workflow.md` keep it on `project/governance`. `AGENTS.md` explicitly scopes that export rule to governance commits.
9. **Verify the commit, not the working tree.** Confirm a clean status, then rerun the full test command at the committed SHA.
10. **Hand off and return.** Write `implementation-ready.md` and update the issue. Return to the prompt.
11. **Fixes.** For each accepted finding, add a failing test where it is behavioural, fix it, commit, and write a new `implementation-ready.md` with the new SHA. Earlier review and QA evidence does not carry over to the new SHA. History already handed off is not rewritten or force-pushed without the orchestrator.

### Upstream boundary check

The branch contains reviewed product code, tests and documentation, not team scaffolding. Run this in the product worktree; change the base only to the agreed review base. Git errors and forbidden paths must fail with nonzero status.

```bash
python3 - <<'PY_BOUNDARY'
import subprocess
import sys
from pathlib import PurePosixPath
try:
    raw = subprocess.check_output([
        "git", "diff", "--name-only", "-z", "upstream/main...HEAD"
    ])
except subprocess.CalledProcessError as error:
    print("BOUNDARY FAIL: Git could not read the review range", file=sys.stderr)
    sys.exit(error.returncode or 1)
paths = [p.decode("utf-8", "surrogateescape") for p in raw.split(b"\0") if p]
if not paths:
    print("BOUNDARY FAIL: empty candidate diff", file=sys.stderr)
    sys.exit(1)
private_dirs = {".agents", ".beads", ".claude", ".codex", ".opencode", ".work", ".confidential", ".entire"}
root_governance = {"AGENTS.md", "BEADS.md", "CLAUDE.md", "CODE-STANDARDS.md", "docs/library-selector-plan.md", "docs/developer/workflow.md", "docs/developer/bootstrap-status.md", "scripts/team-session.sh"}
forbidden = [p for p in paths if private_dirs.intersection(PurePosixPath(p).parts)
             or p in root_governance or p.startswith(("docs/agent-rules/", "docs/project-context/"))]
if forbidden:
    print("BOUNDARY FAIL: governance/private paths", *forbidden, sep="\n", file=sys.stderr)
    sys.exit(1)
print("BOUNDARY PASS: no known governance/private paths")
PY_BOUNDARY
```

This check is deliberately a necessary condition, not PR approval. Reviewer must classify every changed path and inspect its contents and commit history for private data, generated files, unrelated work and additional governance. Legitimate product test/build scripts, ignore rules and user/developer documentation are permitted when reviewed; their directory name alone is not grounds for rejection. Untracked or staged-only work is not covered by a committed-range check. Record both this exit status and the full upstream diff review.

## Complete measurable rubric

Use `PASS`, `FAIL` or `NOT_RUN` for every row, in `implementation-ready.md`. A `FAIL` blocks handoff. A `NOT_RUN` needs a dependency, an owner and a next action. No averaging.

| Gate | PASS threshold | Evidence |
|---|---|---|
| C1 Revision | One exact SHA; clean status; the contract revision named | `git rev-parse HEAD`, `git status --short` |
| C2 Contract | 100% of decisions implemented or listed as deviations with a reason and the architect's answer | Decision-to-file table |
| C3 Red/green | Every behaviour obligation has a recorded red (expected failure) and green; 0 production changes before their test | Red/green table with commands |
| C4 Test integrity | Tests link production code; 0 reimplemented logic; 0 sleeps used to order replies; 0 real credentials, endpoints or user data | Test source, fixture listing |
| C5 Suite | The full suite passes at the committed SHA, 0 failures, 0 skips without a reason, run 3 times to expose flakiness | Commands, counts, exit codes |
| C6 Async safety | Late success and late failure, in reversed order, pass for every reply family the contract names; 0 stale rows, caches or errors | Named tests |
| C7 Data safety | Tests show download records, session and settings unchanged across a switch and a restart, legacy database included; 0 new deletes of download records in the diff | Named tests, diff read |
| C8 Build | Exact SHA compiles for the emulator and for the documented B288 SDK target, or `NOT_RUN` with the dependency | Build log, or dependency |
| C9 Boundary | The check prints `BOUNDARY PASS` and exits 0 at the handed-off SHA; 0 forbidden paths; each `REVIEWED` path has a written reason and the reviewer's confirmation; 0 generated or private content | Full command output with exit code, reviewed-path list, diff read |
| C10 Size and style | `git diff --check` is clean; 0 unrelated hunks; follows surrounding style; no empty catch; errors reach the user or the log | Diff read |
| C11 Docs | Every statement the change made false is updated in the same range; new test and build commands documented | Grep of old statements, diff |
| C12 Handoff | `implementation-ready.md` complete; issue updated with the SHA; 0 claims beyond the evidence; coder results labelled as coder results | The report |

## Definition of done (DoD)

**Ready for review and QA:** C1–C7 and C9–C12 pass at one SHA, C8 passes or names its dependency, and `implementation-ready.md` exists. If no toolchain can run the tests, C3 and C5 are `FAIL`, and the state is blocked rather than ready. Say so with the exact error and write no production code.

**Implementation done:** the reviewer and QA have reported on the final SHA, every blocking finding is fixed with a rerun or explicitly deferred by the orchestrator, and task-owned scratch is cleared. Keep the worktree until the orchestrator removes it.

Not this role's done: review approval, independent QA, the ARM artifact for release, physical InkPad acceptance, pushes, the upstream PR, or closing the delivery epic. Do not claim these from a local run.
