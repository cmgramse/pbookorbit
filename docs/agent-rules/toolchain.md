# Toolchain

Verified installed: git, gh, bd 1.3.2, entire 0.12.2, cmux, Claude Code and Codex. Entire is enabled for Claude and Codex; session uploads are disabled. Seven Codex hooks are trusted; fresh codex exec and claude -p capture/readback passed. Existing pre-install sessions were not retroactively imported. The shared Beads store uses embedded Dolt, no Dolt remote; see BEADS.md. No automatic setup/upgrade commands are implied.

Use scripts/team-session.sh ROLE from an existing terminal in the one PBookOrbit cmux workspace. It resolves the root from git, checks the role skill exists and launches the selected runtime with its complete seed. --print inspects the seed without launching.

Build and test prerequisites are tracked in docs/developer/workflow.md and QA evidence. No successful product build has been claimed. Codex project capability selection remains tracked in pbo-2ee.

Project .codex/config.toml explicitly selects engineering/review/GitHub/browser capabilities and Hindsight/context7/Exa MCP. Other currently inherited plugins/MCP are disabled locally. Personal skill rules remain untouched. Fresh effective catalogue validation and cross-entry-point behaviour remain OPEN under pbo-2ee; running sessions predate this configuration.
