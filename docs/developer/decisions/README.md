# Decision records

A decision record captures a choice that was hard to make and would be
expensive to reverse: why it was made, what else was considered, and what it
costs. It is not documentation of how the system works — that is
`../architecture.md`. It is documentation of *why it works that way*, written
while the reasoning is still fresh.

Write one when a choice will outlive the person who made it: a datastore, a
protocol, a dependency you will be stuck with, a boundary between components,
a deliberate limitation.

## How to add one

Copy the template below into `NNNN-short-title.md`, numbering sequentially
from `0001`. Records are append-only: when a decision is replaced, leave the
old file in place, set its status to `Superseded by NNNN`, and link forward.
Deleting a record deletes the reasoning, which is the only part worth keeping.

## Template

```markdown
# NNNN. Short title in the imperative

- **Status:** Proposed | Accepted | Superseded by NNNN
- **Date:** YYYY-MM-DD
- **Deciders:** who actually decided

## Context

What forced a decision. The constraints, the pressures, what was already true.
Someone reading this in two years has none of today's context — give it to them.

## Decision

What was decided, stated plainly and actively.

## Alternatives considered

Each realistic option and the specific reason it was not chosen. "It was worse"
is not a reason; this section is what stops the same debate reopening.

## Consequences

What this makes easy, what it makes hard, and what it commits the project to.
Include the costs — a record listing only benefits was written to justify, not
to inform.
```
