# Code standards

Follow the existing C++/Qt/QML conventions. Prefer small explicit functions and existing models/helpers. Do not introduce a framework or broad refactor for library selection. Use idiomatic Qt ownership, signals and properties; guard asynchronous results against changed account/library context. Keep networking and selection policy out of QML presentation where existing architecture supports it.

Write comments for invariants and non-obvious decisions, not line-by-line narration. Handle malformed responses and errors explicitly; never signal success without validated data. Never suppress failures to make tests pass. Preserve translation patterns, e-ink performance and keyboard/button navigation. Avoid animations or polling introduced solely for the selector.

Use TDD as defined in AGENTS.md. Tests use actual production logic and deterministic fixtures. Keep the upstream diff focused, free of debug output, dead code, placeholders, unrelated formatting and private configuration.
