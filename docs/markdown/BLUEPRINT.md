# Streaming Markdown rendering blueprint

Status: planning deliverable, 2026-11-17. No renderer implementation, dependency
installation, backend switch, or installed-binary update is part of this change.
This extends the minimum UI; its current ANSI/readline renderer stays operational.
Tracked by tix `a3f86e2`. Implementation is split into dependency-gated tickets.

## 1. Goals and acceptance boundary

- Render headings, emphasis, strong text, lists, quotations, links, inline code,
  fenced/indented code and GFM tables as terminal text with semantic styles.
- Render each streamed prefix promptly, including incomplete code blocks and
  table rows, with a visually complete provisional boundary.
- Revoke speculative formatting when later bytes change the interpretation.
- Preserve the exact source supplied by Cog; never submit, persist, or expose
  synthetic closing syntax as assistant-authored text.
- Keep readline editing, draft/cursor, cancellation, resizing and prompt
  separation working throughout output updates.
- Produce backend-neutral styled rows suitable for ANSI and wide-character
  ncurses; give one backend exclusive ownership of the physical screen.
- Bound rendering work/storage, sanitize terminal controls, and degrade to a
  visibly identified plain-text view when Markdown processing is unavailable.
- Reach the same final frame as a one-shot render of the exact final message,
  regardless of the provider's chunk boundaries.

The first dialect is CommonMark-oriented Markdown plus tables. Raw HTML is
literal text. Links show labels and safe destinations as text; no fetching,
clickable terminal escapes, clipboard integration, image download, or execution.
Images become an alt-text placeholder. Language tags label code; syntax
highlighting, math, diagrams, footnotes and task-list interaction are deferred.
Reference definitions are scoped to a single assistant message, never a session.
User prompts, errors, runtime notices and setup fields remain plain text.

## 2. Functional blocks and cross-check

| Block | Responsibility | Goals served |
|---|---|---|
| Snapshot reconciliation in app | Assign message keys, detect changed prefixes/phase, coalesce render requests, retain public snapshot lifetime rules | Source fidelity, lifecycle |
| Preview + parser adapter | Parse exact prefix at temporary EOF; optionally add conservative, disposable syntax closure | Partial syntax, final convergence |
| Row layout | Turn parser callbacks into bounded UTF-8 runs, styles, wrapped rows and table/code decorations | Readability, width/limits |
| Frame/viewport cache | Cache final messages by key/revision/width/theme; replace active message frame atomically | Redraw, resize, latency |
| Screen + readline bridge | Compose visible message slices, input and status; one terminal owner | ANSI/ncurses, prompt/editing |

Keep these responsibilities in two implementation modules initially:
`markdown.c` and a screen module with backend-specific files as needed.
Snapshot reconciliation and viewport selection stay application coordination,
not another service or Cog adapter. [diagram.svg](diagram.svg) shows ownership.

## 3. Source and preview model

Use the full current message prefix from `cog_tui_runtime_snapshot()`, not the
last delta alone. Its borrowed bytes expire on the next mutating runtime call.
Render synchronously while the borrow is valid; frames own derived sanitized
text and never retain snapshot pointers. Scratch preview copies are bounded and
discarded before returning. Cog remains the authoritative transcript.

The application assigns a stable nonzero UI message key. Existing snapshots use
`node_id=0` for the active aggregate, then a nonzero committed identity. Promotion
must preserve the key; a new turn gets another key. Never identify messages only
by their text length or array index. Source revision tracks actual bytes and
phase, including replacements, equal-length changes and shrinking snapshots.

Each render has `STREAMING`, `COMPLETE`, `CANCELLED` or `FAILED` phase. A cancel
request is still STREAMING until Cog reports its terminal outcome. Final source
is parsed without speculative suffixes in all three terminal phases. The app
shows cancellation/failure separately; a drawn code/table border never implies
that the model finished its answer.

Conceptually, a frame has three kinds of content:

| Origin | Example | Lifetime and meaning |
|---|---|---|
| Source | Words and code actually received | Authored bytes remain in Cog; decoded display is derived |
| Decoration | Indent guides, rule lines, code-panel borders | UI layout, never part of the message |
| Preview | A provisional bottom border, optional temporary inline closure effect | Discard/recompute on every new prefix |

Never write repaired Markdown back into the snapshot, settings, prompt, clipboard
or future request. Future copy/export must use exact source, not rendered rows.
Only display formatting can be optimistic.

## 4. Partial-syntax policy

Prefer parser EOF behavior and layout closure before constructing extra syntax.
CommonMark permits a fenced block to reach document EOF without a closing fence.
A code panel can therefore draw a provisional bottom edge immediately, then move
that edge as code arrives. [CommonMark fenced blocks](https://spec.commonmark.org/0.31.2/#fenced-code-blocks).

GFM tables have no explicit closing-table token. Require a valid header and
delimiter row before treating text as a table; then terminate the visible row
and table at the current EOF. Do not fabricate a delimiter row to turn an
ambiguous pipe-separated paragraph into a table.
[GFM table specification](https://github.github.com/gfm/#tables-extension-).

| Received prefix/state | Streaming display | What may change later |
|---|---|---|
| Incomplete fence opener or info line | Show received text/language hint provisionally; commit fence interpretation after line termination or unambiguous parser result | Additional backticks/info bytes can change the opener |
| Recognized open fenced block | Render literal code with a provisional visual bottom edge; never treat Markdown markers inside as inline syntax | More code extends panel; actual fence ends it |
| Partial closing fence line | Hold/repaint that tail line; avoid committing a block boundary from a fragment | A shorter run or trailing text may be code content |
| Complete fence followed by new prose | Remove provisional state from panel, render following text normally | Styles are reset at the boundary |
| Pipe header without delimiter row | Readable ordinary text, including raw pipes | Valid following delimiter can reclassify both rows |
| Header + partial delimiter row | Remain ordinary text until the delimiter is recognized; if EOF temporarily makes it valid, mark entire block provisional | Later bytes may invalidate it |
| Confirmed table + partial data row | Render received cells, empty visual space for missing cells and a provisional bottom border | Last cell/row/column widths can grow |
| Extra table cells or unsupported nesting | Use a raw rendering of the affected table until a source-preserving representation is proven | Never silently lose authored overflow cells |
| Open inline backtick run with content | Optional equal-length virtual closing run only after unambiguous opener and source/provenance tests pass | New delimiter bytes may resolve or invalidate the preview |
| Open `**strong`, `*emphasis`, or strike extension | Optional suffix closure only in a supported unambiguous paragraph context; otherwise display the unfinished delimiters literally | Final parse can remove speculative style |
| Unfinished link/image destination | Show received label/tail as literal text; never invent a URL or `)` | Completed syntax can become a link/placeholder |
| Open list/blockquote | Use parser EOF closure; draw indent/quote decoration, never invent another item | Later lines can continue nesting |
| Split UTF-8 sequence | Hold at most three valid incomplete suffix bytes with pending metadata | Finish character on next prefix; visibly replace malformed final input |

The inline completion rows are a later optional increment, not a prerequisite
for code/table support. Initial release can leave unmatched inline delimiters
literal. Do not use regex delimiter counts. Escapes, code spans, delimiter run
lengths and nested containers must be understood before synthesizing a suffix.
Uncertain contexts use exact-prefix parsing/plain tails instead of guessing.

If a temporary suffix is needed, construct `P = received_prefix + suffix` in
scratch storage, with an origin range for every inserted byte. Only STREAMING
can use it. An adapter must suppress synthetic text and map resulting styles
back to source/decorations. Start every update from the current received prefix,
never from the previous repaired version. Source/provenance failure disables
suffix completion, not the exact-prefix renderer. Fence EOF handling does not
require this extra scanner or suffix machinery.

Do not freeze an apparently finished paragraph inside the active message:
later reference definitions, setext underlines and table delimiters can change
earlier interpretation. Reparse the bounded active message as a whole. Cache
completed messages instead; optimize active-prefix parsing only after evidence
shows a need and invalidation semantics are documented.

## 5. Parser choice and research gates

Provisional choice: **MD4C behind a private adapter**, with explicit tables and
HTML-disabled options. It is C, supports callbacks and avoids requiring an
application AST. Its API parses a whole input document; callback delivery is
not an incremental streaming-parser contract. Coalesced whole-prefix parsing
is our design. [MD4C project](https://github.com/mity/md4c).

| Candidate | Benefit | Cost / decision |
|---|---|---|
| MD4C | Small C parser, table support, callback layout route | Preferred subject to preview/provenance/latency PoC |
| cmark-gfm | C library with a structured document representation and GFM extensions | Comparison candidate if adapter semantics need a tree; measure footprint rather than assume it |
| Handwritten full Markdown parser | No external parser | Reject: larger correctness burden; keep only narrowly scoped preview hints |

The alternative is [GitHub's cmark-gfm](https://github.com/github/cmark-gfm).
No parser is vendored or linked by this planning change. At implementation,
pin an audited release/commit and license in this repository. Keep its build,
configuration and dependency artifacts independent of Cog.

Before selecting the parser, run a throwaway prefix-matrix probe: fenced code,
escaped pipes, tables, callback text provenance, incomplete inline delimiters,
reference-definition changes, NUL and split UTF-8. MD4C's callbacks do not by
themselves promise precise source positions for every normalized construct;
do not expose exact source offsets based on that assumption. Check its
[public header](https://github.com/mity/md4c/blob/master/src/md4c.h).

## 6. Layout and repaint policy

Parser events become owned rows of UTF-8 runs with column/cell counts and style
bits. These contain no ANSI sequences, curses attributes, `WINDOW *`, `FILE *`
or renderer-specific color numbers. No HTML rendering intermediate is needed.
Each backend maps semantic style bits to supported attributes; monochrome still
has indent, spacing and borders. Reset attributes before status/input.

Prose wraps by display cells; retain intentional paragraph and hard breaks.
Code preserves indentation and line boundaries, expands tabs at fixed tab stops,
and wraps long lines with a decoration-only continuation marker. No bytes are
dropped or treated as code to execute. Use ASCII borders when glyph capabilities
are uncertain. Links and images receive bounded, noninteractive text treatment.

Tables allocate width from the current content and available cells, then wrap
cell contents. Recompute the affected table when a new row grows a column; the
whole table can reflow while streaming. Coalescing bounds visual churn. If
minimum column widths plus separators cannot fit, switch to a vertical record
layout with header/value pairs. A later wide terminal can restore the grid.
Parser-standard treatment that discards extra cells is unsuitable for an
assistant transcript: preflight/provenance must detect overflow or fall back to
the raw table. No claim of full GFM equivalence for that deliberate display policy.

Start Unicode support with UTF-8 decoding, CJK widths and combining marks in a
documented UTF-8 locale. Verify width behavior against terminal emulation and
real terminals. Complex emoji/ZWJ and ambiguous-width behavior need a measured
policy; visibly escape unsupported clusters if reliable placement is unavailable.
Do not split a supported cluster when wrapping, clipping, or placing the input
cursor. Incomplete final sequences are not silently discarded.

Frames are immutable and replaceable. A cached row slice is keyed by message key,
source revision, phase, width, requested row window, dialect, theme and Unicode
width policy. A terminal outcome forces a new frame
even if byte length is unchanged. Width change invalidates layout; drawing uses
the newest matching geometry or retries on `AGAIN`.

The frame exposes a row slice plus total logical rows. Store only the requested
viewport, not a million-cell curses pad. Finalized layout caches have an LRU
budget and are disposable; evicted frames are rebuilt from a fresh public
snapshot. Follow the newest output by default. When the user scrolls, preserve
message identity plus a best-effort row anchor; width/reclassification may reflow
within that message. Exact source-position anchoring is deferred until provenance
is established. Never snap back to the tail merely because another chunk arrives.

Target at most 30 render transactions/second, coalescing provider updates while
servicing input/runtime on every event-loop turn. Finalization/cancel/error is
not delayed waiting for another chunk. Initial Markdown parse cap: 128 KiB per
message, 4 MiB frame/storage budget, nesting 64, table columns 32; view requests
cover at most 300 rows and 1000 columns. These are starting limits to validate,
not performance measurements. MD4C has internal allocations and a synchronous
parse call: UI limits do not provide a hard allocator/time sandbox. Benchmark
adversarial inputs before committing the interactive latency budget. Suggested
target: typical parse/layout <8 ms, input reaction <50 ms on the reference host.

Above limits or on unsupported constructs, request bounded PLAIN viewport rows
from exact source and show a plain-view status. On OOM, retain the last good
frame and show a fixed-buffer notice if possible; do not claim the stale view is
current. If no safe display is possible, restore the terminal and exit cleanly.
Frame eviction, fallback and clipping are distinct from deleting source.

## 7. Ncurses and readline composition

The current `src/app.c` output-cursor/ANSI-scroll-region functions cannot coexist
as physical writers alongside curses. Extract a screen abstraction. ANSI remains
the first backend for migration; ncursesw is a selectable optional backend only
after its input-compatibility gate passes. Use a `static const` backend dispatch
table if dispatch is needed; all screen/frame state stays instance-owned.

Preferred ncurses composition: ncurses owns **all** drawing and terminal modes;
readline owns editing and key interpretation. Its custom redisplay hook copies
`rl_line_buffer`/`rl_point` into the screen's staged input view and marks it dirty.
No callback calls `doupdate()` recursively. The event loop stages output/status
and input windows, then presents once, leaving the physical cursor at input.
Readline documents replaceable redisplay and terminal preparation hooks.
[GNU Readline manual](https://tiswww.case.edu/php/chet/readline/readline.html#Readline-Variables).

Initialize ncurses using explicit UI streams so Cog stdout/stderr stay routed
away from the terminal. `newterm()` accepts those streams.
[ncurses initialization](https://invisible-island.net/ncurses/man/curs_initscr.3x.html).
Queue windows with `wnoutrefresh()` and finish with one `doupdate()`; neither the
Markdown module nor readline's default redisplay may write behind curses' model.
[ncurses refresh](https://invisible-island.net/ncurses/man/curs_refresh.3x.html).

The PoC must check more than the redisplay hook: line acceptance, bell, clear
screen, completion listing, history search, bracketed paste, vi mode, and default
terminal prep/deprep can emit output or change modes. Decide which features to
support or explicitly disable, intercept auxiliary display/bell hooks, restore
all replaced hooks, and verify no uncoordinated bytes reach the terminal.
Read stdin through readline callbacks; do not also consume its bytes with
`wgetch()`. Signal handlers only set flags. App loop handles resize and invokes
backend changes; designate one SIGWINCH/mode owner and restore it on teardown.

Ncurses availability (`pkg-config ncursesw` reports 6.5.20250216 locally) is not
evidence that composition works. Readline is locally 8.2. The integration PoC is
unexecuted and remains a required gate. If it fails, ship the backend-neutral
Markdown path on ANSI, retain readline, and keep ncurses deferred with findings.
This preserves the repository's UI direction without requiring Cog changes.

## 8. Headers, scenarios and implementation gates

Draft interfaces are in [markdown.h](include/cog_tui/markdown.h) and
[screen.h](include/cog_tui/screen.h), outside the production include/build tree.
They describe lifecycle, error results, borrows, phases, viewport bounds and
single-owner screen composition. See [scenario traces](SCENARIOS.md) and
[review record](REVIEW.md) for three design passes and remaining evidence gates.

| Order | Work | Dependency and acceptance gate |
|---|---|---|
| M1 | Prefix corpus + parser/provenance/performance probe | No production work; select/pin parser only with evidence |
| M2 | Readline + ncurses screen-owner PoC | Can be researched separately from M1; decide optional backend feasibility |
| M3 | Implement pure Markdown/frame/layout module | M1; reviewed headers, contract tests, prefix partition/fuzz tests |
| M4 | Extract ANSI screen backend and input view | M2 terminal-ownership findings; keep ANSI behavior, pass current 14 tests and screen regressions |
| M5 | Integrate previews, message reconciliation, viewport/cache | M3 + M4; tests for final convergence, resize, cancellation, stale geometry |
| M6 | Add optional ncursesw backend | M2 + M4 + M5; identical semantic frames, PTY matrix, no writer conflicts |
| M7 | Review, sanitizer/performance/installation gates | All shipping modules; update docs and updater dependency support |

M1 and M2 are independent research tasks. Production layout and screen extraction
may proceed separately only after the shared headers stabilize. Integration is
serial; no worker patches Cog. This document records dependency opportunities,
not a request to spawn implementation agents during this planning task.

The concrete tix IDs and dependencies are in [WORK_QUEUE.md](WORK_QUEUE.md).

Each implementation ticket follows architect review → implementation/contract
tests → critic review → integration → finalize. Before writing `.c`, promote the
draft headers into `include/cog_tui`, amend with PoC findings and repeat the
three-pass header/scenario review if their semantics change. Header compilation
alone does not prove any behavior described here.

## 9. Verification and release criteria

- Compile each header alone under strict C11/Werror (GCC and Clang).
- For every fixture and byte prefix, render STREAMING; finish with exact-source
  COMPLETE. Compare final frame with a fresh one-shot render at the same width.
  Repeat every split point and deterministic/random chunk partitions. The same
  prefix/phase/viewport must yield the same frame independent of chunk history.
- Assert source hash/bytes unchanged; no synthetic closing delimiters become
  authored output; valid text never disappears merely because more arrived.
  Syntax markers may stop displaying when they become formatting.
- Include partial/mismatched fences, nested lists, table delimiter rollback,
  escaped pipes, backticks in cells, reference definitions, incomplete links,
  Unicode byte splits, control escapes, large tables and adversarial nesting.
- Verify cell widths, narrow table fallback, growing table redraw and incomplete
  bottom borders at several terminal sizes. Normalize cell/style snapshots to
  compare backend semantics, permitting declared glyph/capability differences.
- Exercise prompt editing/left/right/kill/accept during stream and resize,
  cancellation/failure/finalization, scroll pause and hooks/modes restoration.
  Extend libvterm PTY tests; inspect one real-terminal run for each backend.
- Run existing tests, contract/integration tests, allocation-failure paths,
  ASan/UBSan, and bounded fuzz/performance corpora. Sanitizer scope must identify
  whether the selected parser is instrumented; installed Cog need not be rebuilt.
- Integrate optional dependencies with CMake/CTest and `cog-update`; preserve a
  documented plain fallback and explicit capability choice. Update docs before
  reinstalling. No paid/live provider request is needed for renderer acceptance.

Planning completion means the contracts and work queue are reviewable. Parser
choice, ncurses composition, latency and Unicode behavior remain evidence gates;
this document does not mark them as implemented or validated.
