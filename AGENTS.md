# cog-tui-minimal

An independent, lightweight C11 terminal UI for Cog.

## Boundary

- Require no changes to the Cog project.
- Use only published Cog APIs intended for UI clients; never include private
  Cog headers, compile Cog source files into this project, or patch Cog.
- Keep this repository's code, dependencies, configuration, tests, and build
  artifacts independent. Record unsupported API needs as scope limitations.

## Workflow

- Follow the `software-blueprint` and `cog-dev` skills, as used for Cog.
- Stay in the current blueprint stage until its decisions are agreed with the
  user. Current goals and open decisions are in `docs/GOALS.md`.
- Track decisions and tasks with tix in this repository.
- Design headers/contracts before implementation; complete at least three
  header/scenario/review passes before implementation.
- Use CMake, CTest, strict warnings, header self-containment checks, contract
  tests, integration tests, and appropriate sanitizer checks.
- Keep ownership explicit, avoid application-owned global mutable state, and
  use static const dispatch tables where state-machine dispatch is needed.
- Keep build trees and logs inside this checkout in `build/` or `build-*/`.
- When isolated worktrees are needed, place them beside this repository under
  `../cog-tui-minimal-worktrees/`; use `codex/` branch names.

## UI direction

- Use readline for prompt editing.
- Treat ncurses as an acceptable rendering candidate pending research.
- Keep dependencies and initial feature scope small.
- Verify prompt editing alongside asynchronous output before selecting the
  rendering design.
