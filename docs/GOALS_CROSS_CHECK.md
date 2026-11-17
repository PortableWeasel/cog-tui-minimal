# Stage 3 — Goals cross-check

Status: cross-check complete; scope refinements ready for user review.

This checks the [agreed goals](GOALS.md) against the six
[functional blocks](FUNCTIONAL_BLOCKS.md). Coverage here means an assigned
responsibility, not proof that Cog already exposes every required API.

## Goal coverage

| Goal | Responsible blocks / project gate | Observable result |
|---|---|---|
| First usable terminal UI; run `cog-tui` | Lifecycle, input, Cog integration, rendering | Launch reaches setup or a ready prompt without repeated flags. |
| Independent repository and workflow | Project build/install gate | Build and run from this repo against a documented compatible Cog installation. |
| Public UI APIs only; no Cog changes | Cog integration; integration review gate | All Cog interactions use published UI contracts, without editing the Cog checkout or its configuration/build. |
| Lightweight scope and dependencies | All blocks; dependency review gate | One UI application; each dependency serves a required capability; no extra runtime service required by this design. |
| C11 and Cog development workflow | Project design/build/review gates | Header-first passes, tix records, strict C11 builds, CMake/CTest, contract and integration checks. |
| Readline prompt editing during streaming | Input, lifecycle, rendering | Incoming output does not block editing or corrupt the draft prompt. |
| Guided first-run provider/endpoint/credential/model setup | Input, settings, rendering, Cog integration | Missing values can be supplied and saved, then applied through supported Cog APIs. |
| Restore settings and become ready on later launches | Settings, lifecycle, Cog integration, rendering | Relaunch restores the selection; readiness is shown only after runtime readiness and activation. |
| Inspect/change settings and retain them | Input, settings, lifecycle, Cog integration, rendering | Edits survive relaunch; saved and active selections are distinguishable. |
| Protect credentials; mask input and diagnostics | Settings, input; rendering/diagnostic review gate | Secrets have restricted local access and are absent from ordinary display, input history, and diagnostics. |
| Connect and show ready/unavailable/lost | Cog integration, lifecycle, rendering | Connection transitions are visible and gate submission. |
| Submit prompt and show incremental response | Input, lifecycle, Cog integration, conversation, rendering | A real prompt produces correctly associated visible response chunks. |
| Keep current multi-turn conversation visible | Conversation, rendering | User and assistant turns remain visible for the running UI within documented limits. |
| Interrupt and show the resulting state | Input, lifecycle, Cog integration, conversation, rendering | An exposed Cog interrupt control produces a visible outcome; a request alone is not reported as success. |
| Exit and restore terminal settings | Lifecycle, input, rendering, Cog integration | Normal exit, interruption, and failure cleanup release owned resources and restore terminal use. |
| Correct configuration/auth/model/connection errors | Owning block, lifecycle, input, rendering | The user sees an actionable cause and can correct relevant settings without redoing all setup. |
| Conditional Cog-managed tool calling | Cog integration, conversation, rendering | If included, a real tool round trip is displayed while Cog owns execution and policy. |

## Minimality check

| Block | Why it remains | Simplest useful form |
|---|---|---|
| Lifecycle and coordination | Makes configure-once startup, responsive interaction, and cleanup work together. | One application coordinator, with explicit readiness and resource ownership. |
| User input | Supplies prompts, setup values, settings edits, interrupt, and exit. | Readline plus a small set of discoverable interactions. |
| Settings and credential persistence | Makes subsequent launches usable without repeating setup. | One saved provider/model selection and its required values; no profile manager. |
| Cog integration | Enforces the decoupling boundary and handles runtime interaction. | One adapter to a verified public UI surface. |
| Current conversation | Keeps streamed turns coherent and supports redraws. | In-memory presentation state for the running conversation, with explicit limits. |
| Terminal rendering | Presents output, prompt, readiness, settings, and errors together. | Plain readable text and minimal status; no rich rendering requirement. |

Every block serves a required goal. None needs an independent library, thread,
or process merely because it appears in this list. Logging, build/install,
testing, and conditional tool display do not need additional runtime blocks.

## Ownership gaps clarified

1. **Failed settings writes:** settings preserves the previous saved values and
   retains edits for correction/retry. Rendering distinguishes save failure from
   activation failure. A saved model must not be shown as active prematurely.
2. **Connection loss during a turn:** integration reports the loss; coordination
   stops new submissions; conversation/rendering retain an unresolved outcome
   until a definitive Cog outcome is available. Do not automatically resend an
   uncertain prompt, which could duplicate real work.
3. **Scope versus UI polish:** the minimum is a single saved selection, current
   in-memory conversation, readable output, and status. Profile management,
   session browsing, persisted transcripts, rich Markdown, replay/reconnect
   recovery, and a separate tool executor are not required for this version.

These clarifications refine existing responsibilities; no seventh block is
needed. They are reflected in the functional-block document.

## Feasibility questions carried forward

- Can existing Cog UI contracts establish readiness and activate the saved
  provider/endpoint/credential/model selection without modifying Cog?
- Which supported startup/attachment path delivers the `cog-tui` experience?
- Which APIs provide prompt submission, streamed output, completion, and
  interruption, and what do their error/ownership rules guarantee?
- How will readline, rendering, resize, secret entry, and event handling share
  terminal control while remaining lightweight?
- Which settings/credential storage method and memory limits satisfy the goals?
- Does Cog's current tool and system-prompt behavior support useful tool calls?

Resolve these through Stage 5 research/PoCs before finalizing headers. A missing
mandatory Cog capability requires an explicit scope discussion, not a private
API dependency or a Cog patch. Tool support remains conditional.

## Result

All agreed goals have responsible blocks or project gates. All six blocks are
needed at the responsibility level. There are no unassigned goals after the
clarifications above; API feasibility remains unverified. Stage 4 can diagram
this boundary after the user reviews Stage 3.
