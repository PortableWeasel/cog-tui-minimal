# Stage 1 — Goals

Status: Stage 1 agreed; continuing with Stage 2 functional blocks. The target
is a minimum viable UI for real work:
run `cog-tui`, configure once, then return directly to a usable prompt.

## Confirmed requirements

- Provide the first working terminal UI for the Cog project.
- Maintain the UI in its own Git repository with its own build and development
  workflow.
- Integrate exclusively through Cog's published APIs intended for UI clients,
  without requiring changes to Cog's source, headers, configuration, or build.
- Keep the UI lightweight, with a small dependency set and minimal initial scope.
- Implement the UI in C11 using Cog's header-first development workflow.
- Provide prompt editing through readline while keeping incoming Cog output
  responsive during editing.
- Start with `cog-tui` without requiring repeated provider/model flags or a
  separate manual setup step on every invocation.
- Guide the user through choosing and saving missing critical settings on first
  run, including provider, endpoint where applicable, credentials or credential
  reference, and model.
- Restore saved settings on subsequent runs and present a prompt ready to send
  when configuration is complete and the runtime is ready.
- Let the user inspect and change provider/model settings from within the UI,
  retaining successful changes for the next launch.
- Protect saved credentials from other local users, mask secret input, and omit
  secrets from ordinary display and diagnostics.

## Development constraints

- Use the `software-blueprint` stages and the `cog-dev` workflow: track work with
  tix, design public headers/contracts before implementation, trace scenarios,
  and review implementation before integration.
- Use CMake and CTest, strict compiler warnings, standalone header checks, and
  appropriate contract/integration and sanitizer checks as implementation begins.
- Consider ncurses for rendering; it is acceptable, not yet selected. Research
  readline/rendering interaction before committing to the terminal design.

## Minimum conversation capabilities

- Connect to a compatible Cog runtime and show whether the connection is ready,
  unavailable, or lost.
- Submit a user prompt and display the resulting assistant response incrementally
  as Cog exposes it.
- Keep the current conversation visible across multiple prompt/response turns.
- Interrupt an active response through an exposed Cog control and show the
  resulting state.
- Exit cleanly and restore terminal settings after normal exit, interruption,
  or connection failure.
- Build and run independently against a documented compatible Cog installation.
- Show actionable configuration, authentication, model, and connection errors
  and allow the user to correct settings without restarting setup from scratch.

## Conditional goal — tool calling

- Support Cog-managed tool calls and display activity, results, and errors when
  existing public UI contracts and system-prompt behavior support a reliable
  end-to-end tool round trip.
- Keep tool execution and agent/system-prompt policy owned by Cog; do not build
  a second agent loop or tool executor into the UI.

Tool calling is desired for real work, but is not yet a mandatory first-release
acceptance criterion. Verify it against the existing Cog installation during
research. Record any prompt/API limitation explicitly before deciding whether
tools ship in the first version; do not require Cog changes to deliver this UI.

## Acceptance boundary

Demonstrate a fresh launch of `cog-tui`, guided configuration, a real multi-turn
conversation with incremental output, and a second launch that restores the
chosen provider/model and is ready for input. Demonstrate changing the selection,
an interrupt, correction of a configuration failure, and a visible runtime
failure. Demonstrate installation and execution without editing Cog.

If tool calling is included, also demonstrate a real tool round trip. Mock
output alone does not establish readiness for real work. Verify all runtime
capabilities against the existing published UI APIs before finalizing contracts.

## Agreed scope

- Deliver the configure-once, prompt-ready minimum scope above.
- Defer session browsing and persistent conversation history beyond the current
  conversation from the initial minimum scope.
- Treat tool calling as conditional on existing Cog capabilities and prompt
  readiness.

## Deferred work

Choose the terminal rendering library, transport, and module boundaries in
later blueprint stages. Verify existing UI APIs before designing an adapter.
Determine how `cog-tui` makes the runtime available (launch or attachment) from
the supported public integration surface while preserving the simple startup
experience. Determine the settings format, credential storage/reference method,
and how selected provider/model values reach Cog through supported APIs.
Validate how readline editing, incremental output, resize, and terminal cleanup
work together. Do not write implementation code before the header/contract
and scenario review stages.
