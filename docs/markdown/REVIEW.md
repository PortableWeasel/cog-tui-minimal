# Header and design review record

This is a planning review of draft contracts. No production `.c` was written.
Reviews were performed sequentially in this task; no independent-agent review
or behavioral validation is claimed. Research sources are linked in BLUEPRINT.

## Pass 1 — initial contracts

Wrote `include/cog_tui/markdown.h` and `screen.h` under this docs directory:
owner-thread parser scratch, owned immutable frames, phase/origin metadata,
explicit input staging, screen lifecycle and no backend types in row layout.
Compared these responsibilities to current `src/app.c`, `runtime.h` and the
installed public `cog_cli/cli.h` display types. Noted active node ID promotion,
borrow lifetime and existing readline singleton bridge. Recorded in tix.

## Pass 2 — scenario trace findings and amendments

Traced ordinary/fence/table/UTF-8/resize/lifecycle paths before implementation.
The 26 traces in SCENARIOS are the resulting complete scenario inventory.

| Gap in first draft | Amendment |
|---|---|
| Screen present accepted one message only | Added scene/slice descriptors to compose final/current/user frames and role labels |
| Size exposed physical width, not usable output geometry | Added output_rows/output_columns and AGAIN for geometry mismatch |
| Whole-message frames could exceed storage on huge output | Added virtual row windows, total row count and bounded slices |
| No explicit parse-free fallback format | Added MARKDOWN/PLAIN choice; plain bypasses parser source cap |
| UTF-8 split had no observable pending state | Added pending_utf8_bytes and final malformed-input policy |
| Preview effect was indistinguishable from final layout | Added preview_applied plus provisional rows and origin metadata |

## Pass 3 — consistency and edge review

Reviewed every declared function for NULL arguments, error outputs, borrow
duration, allocation ownership, phase/width validation and teardown behavior.
Reviewed scanner feasibility: core code/table support uses EOF/decorations;
general delimiter repair is not smuggled into a lightweight regex routine.

Further amendments/clarifications:

- A Markdown row limit must not prevent the raw large-message fallback from
  showing a bounded viewport. Apply logical parse row/nesting/table bounds only
  to Markdown; frame storage/viewport bounds apply to both formats.
- Screen IO failure can have partially changed physical cells. Keep frame/input
  ownership valid, mark the backend for full repaint, or close cleanly; never
  report the old terminal as guaranteed unchanged.
- Input view limits and byte-cursor handling need exact rules. Bound copied
  draft/label/status sizes, replace invalid display bytes, and round a cursor
  inside a sequence to a safe preceding cell without changing readline's buffer.
- No parser-source offset API is promised. Prove provenance before synthetic
  suffix suppression/overflow-table detection. If unavailable, use exact-prefix
  parsing and raw fallback; switch parser only with evidence.
- Final-phase source is always exact. Expected EOF behavior for an unclosed fence
  is valid Markdown; a visual border is not an assertion of source completion.
- Callback parsing alone cannot impose a hard CPU/heap limit on a synchronous
  third-party parser. Defaults are limits/targets to measure, not safety claims.
- Draft headers have no circular includes or terminal-library leakage. Strict
  standalone C11 compile is a required planning check; implementation still
  needs contract tests and the architecture/PoC gates.
- A cache of viewport slices must include the requested row window and Unicode
  width policy in its key, not just the message and terminal width. Amended the
  design to prevent a scrolled viewport from showing a stale cached slice.

## Evidence still required before production integration

| Gate | Status |
|---|---|
| Header/scenario passes | Three design passes complete; draft declarations only |
| Header self-containment, GCC and Clang | Both draft headers passed strict standalone C11 compilation with both compilers |
| MD4C prefix/provenance/overflow/latency probe | Not executed; parser choice provisional |
| ncurses + readline no-hidden-writer proof | Not executed; optional backend unselected |
| Unicode width and real-terminal rendering | Not executed for new Markdown layout |
| Implementation, contract/fuzz/PTY/sanitizer tests | Future implementation tickets |

No routine user approval is needed to run those future authorized gates when
implementation is requested. This task stops at the requested blueprint.
