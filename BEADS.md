# Tracking PBookOrbit

The embedded Dolt store at the main checkout's .beads/ is authoritative locally. Issues use prefix pbo. Git carries issues.jsonl for portable issue history, not a complete database backup. No Dolt remote.

From any worktree run `bd -C /Users/claudius/Developer/pbookorbit prime` and the same -C prefix for issue operations. This explicit route prevents accidental separate databases. In another clone replace that path with its main checkout. Use bd where to verify before writes.

Every issue needs: job to be done, owned scope, dependencies, acceptance checks, evidence and remaining blockers. Claim before execution; reviewers own review outcomes, not implementation completion. Record exact SHAs and distinguish tests, build, emulator and real-device results.

At close, run bd export --help and export current issues to .beads/issues.jsonl, inspect and stage it on project/governance. Keep database backups private under .confidential/backups/ using bd backup after reading its help. Never include Beads in the upstream feature PR.

Fork routing: this checkout sets `git config beads.role maintainer`; verify this after cloning so bd does not route to a personal planning store. Serialize writes to the embedded store; retry after a concurrent writer finishes, never reinitialise.
