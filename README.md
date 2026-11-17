# cog-tui-minimal

The first working terminal UI for Cog, developed in an independent repository.

Status: software-blueprint Stage 2 — functional blocks (proposed for discussion).

See [the agreed goals](docs/GOALS.md) and
[functional blocks](docs/FUNCTIONAL_BLOCKS.md). Use C11 and the same header-first,
ticket-driven C workflow as Cog, with CMake and CTest. Use readline for prompt
editing; ncurses is an acceptable candidate for terminal rendering, pending
research. Keep dependencies and scope small.

The intended executable is `cog-tui`: guide initial provider/model setup, save
critical settings, and open directly at a usable prompt on subsequent launches.
Tool calling is a desired conditional goal pending validation of Cog's current
UI APIs and system-prompt behavior.

## Integration boundary

Use only Cog's published APIs intended for UI clients. Developing, building,
and running this UI must require no changes to the Cog project. Keep UI code,
configuration, dependencies, tests, and build artifacts in this repository.
Do not depend on Cog's private implementation headers or source files.

Verify the available public integration contracts during blueprint research.
If a desired feature is unsupported, record the limitation and revise this
UI's scope without changing Cog.
