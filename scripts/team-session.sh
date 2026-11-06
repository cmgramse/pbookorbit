#!/usr/bin/env bash
# Run inside an existing PBookOrbit cmux terminal, never create a workspace.
set -euo pipefail
role=${1:?Usage: team-session.sh architect|coder|reviewer|qa|orchestrator [--print]}
case "$role" in architect|coder) runtime=claude;; reviewer|qa|orchestrator) runtime=codex;; *) echo 'Unknown role' >&2; exit 2;; esac
common=$(git rev-parse --path-format=absolute --git-common-dir)
root=$(dirname "$common")
skill="$root/.agents/skills/pbo-$role/SKILL.md"
[[ -f "$skill" ]] || { echo "Missing role skill: $skill" >&2; exit 1; }
prompt="Read $root/AGENTS.md, $root/BEADS.md, $root/docs/developer/workflow.md and $skill. You are the $role. Read the root Beads store with bd -C $root. Confirm your branch and ownership before edits. Follow the skill rubric and definition of done. You are not alone; preserve peers' edits. Report acknowledgement and current gate to $root/.work/library-selector/$role-ack.md. Keep governance out of the upstream feature branch."
if [[ ${2:-} == --print ]]; then printf '%s\n' "$prompt"; exit 0; fi
command -v "$runtime" >/dev/null
exec "$runtime" "$prompt"
