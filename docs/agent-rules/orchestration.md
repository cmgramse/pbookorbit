# Orchestration

## Three-Agent Principle

The orchestrator, executor, and reviewer have different jobs. Keeping them
separate is what stops an agent from marking its own homework.

## Orchestrator

- Defines goal achievement before dispatch.
- Writes acceptance criteria that a reviewer can evaluate.
- Splits work into scoped items with clear boundaries.
- Passes only the necessary context to each executor.
- Records ownership, progress, and decisions in the project's task record.

## Executor

- Claims the assigned work item in the project's task record.
- Implements only the assigned scope.
- Runs the agreed verification.
- Reports changed files, evidence, and unresolved risks.

## Reviewer

- Reviews against the orchestrator's acceptance criteria.
- Checks behavior, regression risk, security, test coverage, and scope drift.
- Provides findings before summaries.
- Does not rewrite the goal after implementation.

## Dispatch Gate

Do not dispatch an executor until success criteria, acceptance criteria, scope boundaries, and verification evidence are written down.
