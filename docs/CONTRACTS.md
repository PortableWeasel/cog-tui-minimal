# UI contract inventory

All APIs are single-owner-thread, non-reentrant, and return `cog_result_t`.
No runtime owns terminal descriptors. The application routes Cog stdout/stderr
before construction and restores descriptors after cleanup. Only constructors
allocate runtime ownership; settings buffers are caller-owned fixed arrays.
Rendering contexts and immutable frames have their own explicit lifetimes.

| Header | Types | Functions | Critical invariants |
|---|---|---|---|
| settings.h | cog_tui_settings_t | defaults, validate, load, save | No raw credentials; bounded arrays; load output unchanged on error; old file preserved before rename. |
| runtime.h | opaque runtime, options, status | new, submit, step, snapshot, cancel, reload, shutdown, free | No private Cog calls; one manual driver; snapshot borrows expire on mutation; shutdown drains before free. |
| markdown.h | renderer, frame, source/phase/window, rows/runs | new, render, frame_view, frame_storage, frame_free, free | Exact source untouched; backend-neutral sanitized UTF-8 rows; independent immutable frames; checked limits and actual storage accounting. |
| screen.h | screen, geometry, input, slices/scene | open, resize, input, present, close | Sole terminal painter; explicit borrowed streams; copied draft; validated bounded scenes; AGAIN on resize; screen modes restored. |
| transcript.h | transcript/cache owner | new, sync, compose, scroll, fail, identity, free | Stable UI identity across unique active promotion/replay; exact-source cache comparisons; no retained snapshot borrows; bounded cache and viewport frames. |
| app.h | application entry | run | Runtime service independent of paint cadence; readline bridge and signal handlers have bounded lifetime; descriptors/hooks/key entries/modes restored. |


Runtime wrapper functions carry the `cog_tui_runtime_` prefix and settings
functions carry `cog_tui_settings_`. Exact argument/error contracts are in headers.
Provider choice is OpenRouter in this release; mock is explicitly a test mode.
Tools and persistent Cog sessions are disabled. The current conversation remains
authoritative in Cog's app and its public display snapshot; the UI retains only
bounded derived frames and exact source copies for cache comparison.

Terminal rendering uses one ANSI screen painter with absolute cursor placement.
It composes immutable Markdown message slices above the bottom status/separator
and a single input row. The final physical column is reserved to avoid autowrap;
output geometry is capped at 300 rows by 1,000 cells. Tiny terminals get a clipped
resize notice. Screen presentation validates all scene rows/runs before writing;
geometry changes return AGAIN for reflow. IO can leave a partial physical repaint,
so the caller retries a full repaint or closes cleanly.

Readline owns the draft and byte cursor. Its custom redisplay stages copied input
and latches errors; it does not paint recursively. Auxiliary readline output goes
to an owned sink and bells are disabled for the session. The app restores hooks,
streams, bell policy and modified key entries. Ctrl-P/Ctrl-N page the viewport,
and Ctrl-T returns to live tail without mutating the draft. Bracketed-paste mode
is saved/enabled by the terminal owner and explicitly disabled before attempted
restore. Terminals supporting private-mode save/restore preserve the prior mode;
others safely return disabled. The app enables readline parsing
for its session even if inputrc initially disables it, then restores that setting.

The Markdown renderer reparses the complete bounded active prefix, allowing later
syntax to reclassify earlier text. Open code/table EOF boundaries are reversible
visual decoration; authored source is never repaired, persisted or resubmitted.
Final COMPLETE/CANCELLED/FAILED phases render the exact source without provisional
closures. Ambiguous inline syntax stays literal. LIMIT/PROTOCOL falls back to
plain text with a visible notice. Unsupported emoji/format characters are escaped;
CJK and supported combining output use the caller's UTF-8 locale.

The transcript reconciles every service tick, assigning nonzero stable UI keys
and matching durable IDs within one session. Unique active-to-committed promotion
uses the admitted user predecessor. Duplicate durable IDs or unexpected session
changes fail explicitly. Cache keys include exact source bytes, phase, width and
row window; equal-length replacements and shorter final text invalidate frames.
The cache owns at most 4 MiB including metadata, copied source and exact frame
allocations, excluding allocator bookkeeping. Uncached visible frames have a
separate 4 MiB aggregate cap. Oversized source bypasses caching without truncation.
Returned scene borrows end on transcript mutation; Cog snapshot pointers are
never retained. Paused scrolling anchors to UI key/logical row and survives
same-session replay and reflow, with a visible notice if the anchor expires.

Output paints coalesce at 34 ms while runtime state advances each service tick;
input, resize, errors and terminal phases request immediate presentation. A fatal
runtime error marks any remaining active prefix FAILED without inventing a durable
ID. OOM/rendering errors permit clean teardown rather than a falsely current frame.
Ncursesw is not implemented in this release; requesting that backend returns
UNSUPPORTED. Ownership and contract reviews are recorded in `docs/markdown/`.

Readiness means the local runtime accepted configuration, not a successful future
network request. Reload success means published for a future turn, not activated.
Visible outcome uses public message status, never undocumented numeric CLI state.
A submit return of OK means queued; local UI busy state is cleared only after a
new committed terminal assistant display item (nonzero node ID) or explicit
runtime failure, not on submit. Active aggregates may already be INCOMPLETE
with node_id=0; status alone is insufficient. Verify this in runtime tests.

Settings format: five lines, `version=1`, `provider=...`, `model=...`,
`base_url=...`, `credential_ref=...`. Writer emits that order. Reader accepts any
order exactly once, LF terminated (final LF optional), rejects blank/extra lines,
unknown keys, duplicates, embedded NUL/CR, oversized content, incomplete selection.
Credential references match `env:[A-Za-z_][A-Za-z0-9_]*`. Values have no ASCII
controls. HTTPS URL must have a nonempty host and no userinfo, query or fragment.
No credential value is loaded until the Cog constructor/reload resolver needs it.

Tests must cover every row of the scenario document plus strict standalone
headers. Atomic save errors before replacement retain the previous file. A
post-rename directory durability error is reported as indeterminate, because the
new file may already be visible. No operation expands shell syntax.

A snapshot failure after successful core service may leave the drive running;
status.done=false requires shutdown and continued stepping before free. Core
driver failures instead latch done. Neither path authorizes replay.
