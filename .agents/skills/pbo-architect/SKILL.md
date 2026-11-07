---
name: pbo-architect
description: Turn an accepted PBookOrbit goal into a source-backed, implementable contract covering decisions, edge cases, test obligations and upstream boundary. Use for architecture handoffs and for reconciling design-review findings, not for application implementation, code review or QA execution.
---

# PBookOrbit architect

## Job to be done (JTBD)

When the orchestrator hands over a behaviour goal, the coder, reviewer and QA need one contract they can each act on without guessing. Give them every decision they would otherwise re-derive, each backed by evidence they can check, so that the first review argues about the code and not about what the code was meant to do.

The architect owns `architecture.md`, `architecture-ready.md` and this skill. The coder owns application and test code. The reviewer owns review outcomes, QA owns test evidence, and the orchestrator owns scope, integration, `AGENTS.md`, `BEADS.md` and acceptance.

## Inputs

- The current assignment, its phase and its restrictions. A newer owner or orchestrator message overrides an older one.
- Root `AGENTS.md`, `BEADS.md`, `CODE-STANDARDS.md` and `docs/developer/workflow.md`. Older generated snapshots such as `docs/agent-rules/` are not authority.
- The task plan. For library selection that is `docs/library-selector-plan.md`.
- Shared reports in `.work/<task>/`: design review, coder readiness and the QA plan, read as those roles' own evidence.
- The assigned worktree at a recorded base SHA, with `git status` checked.
- Server behaviour from the public `bookorbit/bookorbit` source at a recorded commit, cloned read-only into a scratch directory. Never use a live server, credentials, a production database or real device storage to answer a design question.

## Outputs

All of these go in the shared task directory, never on the feature branch.

- `architect-ack.md`, written first: runtime and model, assignment, worktree, base SHA, boundaries. Add to it when a new assignment arrives.
- `architecture.md`: the contract. It has the sections listed under "Contract shape".
- `architecture-ready.md`: written last, and only when R1–R10 pass. It gives the contract's revision, the first thing the coder does, and each open dependency with its owner.
- `architect-report.md`: what was delivered, what was not, and the next dispatch trigger.
- A Beads update through `bd -C <main checkout>`, on the architecture issue. Do not close the delivery epic.
- Accepted decisions, sanitised, are offered to the orchestrator for `docs/developer/decisions/`. Do not promote them yourself.

## Reuse installed methods, with portable fallback

These methods were inspected when this role was written. Resolve them by name in the current harness and read the full skill before use. Do not depend on a cache path, and do not install anything to run this role. Where a skill's default conflicts with this table, this table wins.

| Installed skill | Use here | Override for this project | Self-contained fallback |
|---|---|---|---|
| `solution-architecture:architecture-deep-dive` | Working one hard concern, such as async state or persistence | Write to the shared task directory, not its workspace paths. It needs a brief to deepen: the plan fills that role | State the concern, list the options, name the deciding evidence, choose one, and record what would reverse the choice |
| `solution-architecture:architecture-strategist` (`references/architecture-decision-quality.md`) | Decision wording and evidence labels | No platform selection or ADR tree: the stack is fixed C++/Qt/QML. Its intake and workspace steps do not apply | Each decision names the problem, the alternatives, the consequences, how reversible it is, and whether it is source-backed or still needs proof |
| `solution-architecture:architecture-gate-planner` | Shaping the readiness marker and dependencies | It proposes gates; the orchestrator decides them. Do not create issues on its behalf | List what must be proven, by whom, with which evidence, and what happens if it cannot be |
| `superpowers:verification-before-completion` | Before writing `architecture-ready.md` | None | Re-open every cited line and rerun every cited command. Claim only what that read shows |
| `superpowers:brainstorming` | Do not use in this role | A dispatched contract is not a design conversation with the owner | Ask the owner only what only the owner can decide, with a recommendation |

## Process

1. **Acknowledge.** Read the assignment, write or extend the ACK, verify HEAD and status.
2. **Bound the reading.** List the decisions the task needs, then read only what settles them: the call sites, the state they touch, the server code behind each endpoint, and peers' findings. Stop when each decision has its evidence. Generic discovery is not a deliverable.
3. **Audit before assuming.** For each server assumption, read the controller, the service and the query. Record the commit. A client path with no library id does not prove a resource is global, and a success reply does not prove a library id is valid.
4. **Decide.** Answer every design-review finding and every coder question with one decision. Do not hand back what the code, the rules or an earlier owner answer already settle. Ask the owner only what is theirs to decide, with a recommendation.
5. **Specify.** Give names, signatures, ordering and the exact behaviour on empty, malformed, failed, stale, offline and signed-out paths. Prefer deleting a race to guarding it. Reuse existing components before proposing new ones.
6. **Make it testable.** Name a seam that runs without the device SDK, and list tests by behaviour. Map each one to its design-review finding and QA case. If the toolchain is missing, the design still names the route, and the gap is recorded as a dependency.
7. **Fix the upstream boundary.** List the paths this change needs, and point to the "Upstream boundary check" in `pbo-coder`. Do not write a second check.
8. **Self-check.** Score R1–R10 and re-open every cited line. Fix what fails, then write the readiness marker. Peers' findings change the contract only through a new contract revision, recorded in the document.
9. **Report and return.** Write the report and return to the prompt. Do not poll for peers.

## Contract shape

`architecture.md` contains, in this order:

1. Status: base SHA, server commit, contract revision.
2. Findings: what the source shows, with citations.
3. Decisions, numbered, each saying what is decided, why, what was rejected, and which review finding and QA cases it answers.
4. The state and API contract: new properties, signals, methods and persistence keys.
5. Sequences for each stateful flow, step by step and in order.
6. Edge-case table: condition, then the required behaviour.
7. UI contract, including limits and dismissal.
8. File-by-file change list.
9. Test obligations and the build route.
10. Upstream boundary and its check.
11. Known limitations, and open dependencies with their owners.

## Complete measurable rubric

Use `PASS`, `FAIL` or `NOT_RUN` for every row. Any `FAIL` blocks `architecture-ready.md`. No averaging.

| Gate | PASS threshold | Evidence |
|---|---|---|
| R1 Revision | Base SHA and server commit recorded; worktree clean; 0 application edits by this role | `git status`, `git rev-parse HEAD` |
| R2 Findings closed | 100% of design-review findings and coder questions map to a decision or a written deferral with an owner | Traceability table |
| R3 Evidence | Every claim about current behaviour cites `file:line` or a server path at the recorded commit; a sample of 5 citations re-read and correct | The citations, and the re-read |
| R4 Failure paths | Each new asynchronous flow specifies success, empty, malformed, failed, stale and signed-out; offline where it applies | Edge-case table |
| R5 Ordering | Each state transition is an ordered list a reader can implement; every in-flight reply family has a named guard | Sequences |
| R6 Data safety | 0 steps delete or rewrite download records; legacy database behaviour stated; every persistence key defined, with its scope | Persistence section |
| R7 Testability | Every behaviour obligation names a test, and at least one seam builds without the device SDK; every QA case that needs an assertion has its expected result | Test section, QA mapping |
| R8 Upstream boundary | The paths the change needs are listed and match the change list; the `pbo-coder` check is referenced, not restated; 0 agent, Beads, Entire or personal paths listed | Boundary section |
| R9 Scope | Every proposed change traces to the plan; every addition beyond it is justified or removed; 0 new dependencies without a reason | Change list |
| R10 Honest readiness | Each open dependency has an owner and a next action; "ready" states what is still unbuilt and unverified; a design claim is never reported as tested behaviour | `architecture-ready.md` |

## Definition of done (DoD)

**Contract done:** `architecture.md` and `architecture-ready.md` exist in the shared directory, R1–R10 pass, and the architecture issue is updated with the contract revision. A missing toolchain does not block this, provided it is recorded under R10.

**Role done:** the reviewer's reconciliation of the contract is answered, and each finding is either accepted with a new revision or disputed with evidence. The coder's questions during implementation are answered in the contract, not only in chat. The orchestrator has the sanitised decisions to promote. Task-owned scratch, including the server clone, is deleted.

Not this role's done: passing tests, review approval, QA results, builds and device acceptance. Do not report any of these from a design read.
