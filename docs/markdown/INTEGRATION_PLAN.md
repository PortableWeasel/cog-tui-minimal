# M5 integration contract and review

This is the preimplementation review for ticket `d3b34a7`. It applies to the
application compositor, after M1–M4 establish the renderer and screen contracts.
The three passes below were performed by a reviewer against the existing
`src/app.c`, public installed `cog_cli/cli.h`, and the draft Markdown/screen
headers. They are design reviews, not claims of executed integration tests.

## Boundaries and ownership

Cog remains the owner of conversation content and turn execution. The app reads
only `cog_tui_runtime_snapshot()` and the runtime control APIs. No Cog source,
private header, or additional provider loop is needed. The Markdown module owns
immutable frames; the screen borrows their views only during `present()`.
Readline owns the draft and byte cursor. The screen is the sole physical writer.

Snapshot text, item pointers, and session pointers expire at the next runtime
mutation. Reconcile and render within the current borrow, or copy exact bytes
into explicitly bounded cache storage. Never retain borrowed pointers in a cache.
The compositor's state belongs to `app_t` or an owned module, with only the
existing bounded readline callback binding and signal flags as process bridges.

## Pass 1 — identity, phase, and scheduling

Replace the current parallel arrays indexed by display position with explicit
message records. Each record has a nonzero monotonically assigned UI key,
optional durable node ID, role, exact source revision, phase, and owned cache
entries. Array position may determine order, but cannot establish identity.

For committed messages, match the saved session ID and durable node ID. Reject
an unexpected session change in this single-session client instead of mixing
conversations. Snapshot `generation` is a replay lifetime identity, not the
durable message identity or the reload configuration ID. When it changes,
reconcile the complete fresh snapshot and invalidate replay assumptions; retain
keys for matching durable nodes in the same session. Do not erase conversation
or scroll anchors merely because a replay generation changed.

The active `node_id=0` aggregate receives a UI key when first observed. Its key
is scoped to the admitted turn and its last known durable predecessor. On the
next full snapshot, transfer that key to the unique new committed assistant
item belonging to that turn. Do not require the final text to equal the last
streamed text: finalization may replace or shorten it. A fresh turn receives a
fresh key. Ambiguous promotion must discard the uncertain cache and preserve
source through a fresh render; it must not associate an old frame with another
message. If the previous active key was a scroll anchor and cannot be resolved,
show an explicit anchor-expired notice and retain the nearest surviving
message, rather than silently jumping to live output.

Phase mapping is separate from provider progress:

| Public item | Renderer phase |
|---|---|
| Active aggregate, node ID zero | STREAMING, including status INCOMPLETE |
| Committed item, COMPLETE | COMPLETE |
| Committed item, CANCELLED | CANCELLED |
| Other committed assistant outcome | FAILED |
| User message | COMPLETE, PLAIN format |

Requesting cancellation does not change phase. Only the observed terminal
snapshot removes provisional rendering. On a fatal runtime outcome without a
committed aggregate, render any last authoritative active prefix as FAILED and
label the runtime failure; never invent a successful completion or retry input.

Process busy-state changes and terminal outcomes on every service tick. The
old app does this inside `render()`; separating reconciliation from presentation
prevents a 30 Hz redraw limit from delaying cancellation or accepting a prompt
against obsolete state. Coalesce changed active prefixes; input, resize,
terminal-phase, and error notices request an immediate presentation. Never
sleep in parsing or callbacks. Use a monotonic clock for render deadlines.

Source revision compares bytes and length, not length alone. A phase change
also invalidates the rendered frame. A hash may accelerate comparison but is
not proof of byte equality. If retaining exact source exceeds the cache budget,
do not cache that message: re-render from the fresh public snapshot instead.

## Pass 2 — viewport, cache, and failure scenarios

Build one scene from visible message slices, plain role labels, bounded plain
notices, and the staged draft. Render from the newest message backward until
the output viewport is filled, accounting for each label row. The renderer's
`total_rows` and `first_row` describe its virtual slice; never allocate a whole
conversation-sized terminal pad. Trim slices by changing borrowed row windows,
not by modifying immutable frames. Release every replaced frame after scene
borrows end.

Follow-tail is initially enabled. Explicit scroll input disables it and stores
the top visible message key plus logical row offset. New chunks preserve that
anchor. Width changes or Markdown reclassification reflow the message and clamp
the row offset as necessary; exact source-offset anchoring is not promised.
Provide an explicit return-to-tail action and visible paused indicator. Paging
must not mutate or submit the readline draft. Implement user actions through a
documented UI binding or command and restore any changed readline bindings.

A cache key includes UI message key, exact revision, phase, format, output
width, requested row window, and renderer policy revision (dialect, theme,
Unicode width). Cache finalized messages; any active-prefix cache is only a
frame reuse for the identical prefix, never a supposedly stable parsed prefix.
Apply an LRU budget to frames plus copied source and metadata. Begin with a
4 MiB aggregate cache budget, separate from the renderer's per-frame bound.
If precise frame allocation size is unavailable, use conservative checked
accounting from row/run/text payloads plus allocation overhead, or add an
allocation-size query to the contract before relying on a hard aggregate cap.
Cache eviction cannot remove Cog conversation text. Oversized entries bypass
the cache; the current visible frame is separately bounded and short-lived.

| Scenario | Required response / gap resolved |
|---|---|
| Same-length edit or shrinking prefix | Exact revision changes; replace frame instead of append-delta logic. |
| Commit with unchanged bytes | Phase invalidates frame; remove previews, preserve UI key. |
| Replay generation changes, same durable nodes | Full reconciliation; retain keys and conversation anchor. |
| Scroll paused while active table widens | Re-render anchored message; no automatic tail snap. |
| Terminal resize between layout and paint | `AGAIN` triggers resize/reflow from current source; do not loop without servicing input. |
| Markdown LIMIT or PROTOCOL | Render PLAIN from exact same source/window and show a plain-view notice. |
| Markdown or fallback allocation failure | Keep last good frame with a fixed-buffer stale notice, or restore terminal and exit if no safe display exists. |
| Cache entry cannot fit | Render uncached, then release; do not truncate authoritative source. |
| Tiny terminal | Honor screen's zero output geometry; skip invalid Markdown window requests and show compact screen-owned notice. |
| Multiple status events between paints | Preserve actionable errors in a bounded notice queue, not a single overwritten string. |

Notices need their own bounded policy because startup currently emits help,
ready, and model messages consecutively. A single scene status string would
otherwise erase all but the final event before the first presentation. Keep a
small fixed notice ring (for example eight entries, 512 bytes each), represent
older notices as plain output rows while space permits, and reserve the scene
status for the latest current condition. Split or visibly truncate long notices
within bounds; never silently discard the only actionable setup error.

## Pass 3 — input, teardown, and executable gates

The M2 screen PoC determines the exact readline hook ownership before app
implementation. The redisplay bridge stages a copied prompt/draft/byte cursor
and marks the screen dirty; it never paints recursively. Hidden readline writes
(accept, completion, search, bell, clear, prep/deprep) must be intercepted,
suppressed through an owned nonterminal stream, or explicitly unsupported with
tests. Do not send readline's default renderer to the UI terminal while the
screen owns it. Do not add a second stdin reader. Save/restore hooks, streams,
signal policy, terminal modes, and any key bindings on every exit path.

Screen input staging errors must reach the event loop through a latched error;
callbacks have no result channel. Do not keep painting an old draft as though
it were current. Screen IO errors terminate safely; `AGAIN` is retryable only
after geometry refresh. Cog stdout/stderr remain redirected before construction,
and screen opening uses the explicitly saved UI stream.

Required executable integration cases before M5 closes:

1. One label per message and no text duplication through active-to-durable
   promotion, including final replacement and zero-length output.
2. Same-length and shorter prefix replacements, and phase-only completion.
3. Complete/cancel/failure frames equal direct exact-source final renders.
4. Draft edits, byte cursor, paste, settings prompts, and scrolling survive
   asynchronous output and resize; no callbacks paint recursively.
5. Scroll-pause anchor persists through new chunks, reflow, and same-session
   replay generation changes; explicit tail action resumes following.
6. Mixed final/user/active frames compose within output geometry with style
   reset before labels, notices, separator, and input.
7. Plain fallback, cache eviction/oversized bypass, allocation failure, tiny
   geometry, and `AGAIN` preserve source and clean teardown.
8. Startup/setup/error notices remain actionable across coalesced frames.
9. Runtime state continues advancing while redraws are coalesced; terminal
   outcomes and shutdown do not depend on receiving another provider chunk.
10. GCC strict/header checks, CTest PTY screen assertions, and Clang ASan/UBSan
    pass against the installed public Cog API only.

The implementation should extract reconciliation into a pure module if that is
needed to inject synthetic public snapshots for cases Cog's deterministic mock
does not produce. Such a module needs a contracted header and the same three
review passes before code. Do not add production test-only provider behavior or
patch Cog to manufacture those cases.

## Separate live-provider acceptance

Ticket `7901f1a` is independent of Markdown correctness. Its earlier condition
was explicit opt-in for live requests; this plan sends none. Once opted in, use
the existing Pi launcher with a temporary private `COG_TUI_CONFIG_DIR` and the
new binary through `COG_TUI_BIN`, so credentials stay in the child environment
and saved user configuration is untouched. Never print environment variables,
auth JSON, or raw provider diagnostics.

Run a PTY session with two short prompts: first request a small numbered list
and a unique innocuous marker, then ask for that marker to establish multi-turn
context. Observe actual changing assistant prefixes, a committed complete
outcome, editable draft preservation, clean shutdown, and restored terminal
modes. Relaunch from the generated private settings and verify readiness.
A separate longer bounded response can exercise cancellation after visible
output, followed by a short fresh turn. Do not infer cancellation from the UI
request acknowledgment alone. Existing offline malformed-credential and local
TLS-failure tests remain deterministic failure coverage; an external rejected
authentication probe needs no real secret and is unnecessary for the happy-path
credential test. No test should automatically retry an uncertain live request.

The current runtime caps each turn at 4096 output tokens and 120 seconds, with
zero retries; even a short prompt cannot guarantee an exact monetary charge.
Tools remain deliberately disabled. Public reload reports publication for a
future turn, not proof of the activated model. The snapshot is display content,
not an API for inspecting private provider history. These limits must remain
explicit in acceptance evidence.
