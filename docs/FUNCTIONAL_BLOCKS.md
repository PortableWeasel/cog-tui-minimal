# Stage 2 — Functional blocks

Status: Stage 2 baseline; refined by the [Stage 3 cross-check](GOALS_CROSS_CHECK.md).
These are categories of responsibility, not a commitment to six libraries,
processes, or source modules. Keep one lightweight UI application; resolve its
internal module boundaries and public contracts in later stages.

## 1. Application lifecycle and coordination

Own startup, the path from setup to a ready prompt, and shutdown. Coordinate
terminal input, incoming Cog events, interruption, resize, and exit so streamed
output continues while the user edits. Choose when a request is allowed based
on configuration, connection, and active-turn state.

Track resource ownership and restore the terminal on exit. Runtime launch versus
attachment is a later integration decision; launching `cog-tui` must provide the
agreed simple startup experience. Any runtime shutdown must respect whether the
UI owns that runtime.

On connection loss, stop admitting submissions and show the current turn as
unresolved unless Cog has supplied a definitive outcome. Do not automatically
resubmit a prompt whose acceptance or completion is unknown.

## 2. User input and interaction

Accept editable prompts through readline and provide small, discoverable
interactions for setup, inspecting/changing settings, interrupt, and exit.
Collect provider, endpoint where needed, credentials or a credential reference,
and model; mask secret entry and keep it out of ordinary input history.

Produce user intentions for the coordinator rather than executing provider
requests directly. Preserve a draft prompt across incoming output and redraws.
The same terminal surface can serve setup and conversation; a separate settings
screen or menu framework is not required by this block.

## 3. Settings and credential persistence

Own UI-specific saved settings, defaults, local validation, and credential
retrieval/storage or references. Load a saved selection on startup, identify
missing values, and save user-confirmed edits with clear failure reporting.

Keep incomplete edits separate from the saved selection, protect stored secrets
from other local users, and omit credentials from ordinary display and
diagnostics. Distinguish saved preferences from the configuration actually
active in Cog. File format, credential mechanism, validation depth, and timing
of configuration changes remain research/contract decisions.

If saving an edit fails, retain the previous saved configuration and keep the
edit available for correction or retry. Report save failure separately from
failure to activate a successfully saved selection in Cog.

## 4. Cog integration boundary

Own all interaction with Cog through its published UI-facing APIs. Establish
runtime readiness, apply or select provider/model configuration where supported,
submit prompts, request interruption, and translate incoming output, completion,
status, and error information into UI updates.

Report unsupported configuration/control capabilities explicitly. Do not silently
fall back to another provider or model. Keep provider HTTP calls, agent policy,
system prompts, and tool execution in Cog. Do not implement a second agent loop.
The exact API, transport, compatibility checks, and runtime discovery/launch
method must be verified later; this block does not assume they already exist.

## 5. Current conversation and presentation state

Hold the visible conversation for the running UI: submitted user messages,
incremental assistant text, turn completion/interruption/failure, and status
needed for redraws. Track enough request/turn association to keep output with
the right visible turn rather than conflating a local submission with acceptance
or completion by Cog.

If tools are included, represent Cog-reported activity/results here as well.
Cog remains authoritative for execution and session state. This block is a
local presentation model, with no session browser, persistent transcript store,
conversation branching, or replay engine in the minimum scope. Memory limits
and visible-history retention need explicit contracts later.

## 6. Terminal output and rendering

Display conversation output, the editable prompt, active provider/model,
connection/turn status, setup interactions, and actionable errors. Preserve
readable output as chunks arrive and when the terminal resizes. Clearly
distinguish ready, busy, interrupted, and unavailable states.

Coordinate terminal ownership and redraw with readline; do not let independently
writing components corrupt the prompt. Ncurses is an acceptable candidate,
pending validation of its interaction with readline. Rendering layout, Markdown
support, and full-screen behavior are not selected at this stage.

Show the active provider/model reported or established through Cog integration;
label a newly saved selection as pending until activation is confirmed. Present
connection loss or an unknown turn outcome without implying successful completion.

## Responsibility flow

On launch, lifecycle coordination loads settings. Missing settings route through
input and rendering for setup; a complete selection routes through Cog
integration to establish readiness. Rendering presents a prompt when ready.

For a conversation turn, input produces a submission, the coordinator forwards
it through Cog integration, and incoming Cog events update conversation state.
Rendering presents those changes while input remains responsive.

For a settings change, input collects edits, settings validates and saves them,
and coordination asks Cog integration to activate them through supported APIs.
Display whether settings are saved, active, or failed to activate. Never imply
that saving a selection means Cog has accepted it.

## Cross-cutting responsibilities

Errors are handled by the block that owns the failed operation and surfaced
through coordination and rendering, with enough context to correct a setting
or understand a runtime failure. Minimal diagnostics must exclude secrets.
This does not require a separate logging service.

Build/install and verification remain project-wide responsibilities: independent
CMake/CTest workflows, strict C11 compilation, header checks, contract tests,
and real Cog integration checks. They are not runtime subsystems.

## Scope boundary and research questions

- Verify whether the current public UI surface supports runtime readiness,
  provider/model configuration, prompt submission, incremental output, and
  interruption without any changes to Cog.
- Resolve readline/rendering/event coordination and credential persistence.
- Decide how to handle a settings change during an active turn using supported
  Cog behavior; avoid promising hot reconfiguration before checking the API.
- Include tool activity rendering only if existing APIs and system prompts
  support a reliable tool round trip. No separate tool subsystem is needed here.

The Stage 3 cross-check maps every agreed goal to these responsibilities and
records the remaining feasibility questions before the diagram and research
stages.
