# Renderer production contract review

Before implementation, the prior three draft review passes were rechecked:

1. Ownership/API pass: promote the existing immutable frame API unchanged;
   locale remains caller-owned, no global mutable state. Returned frames outlive
   context teardown. All source pointers are call-only borrows.
2. Scenario pass: overflow tables must return PROTOCOL for exact-source plain
   fallback because MD4C discards extra cells. Conservatively checking all pipe
   lines may choose plain view for some valid messages, never delete cells.
   Entity spelling stays literal; no partial homegrown entity decoder.
3. Bounds/Unicode pass: hold valid incomplete UTF-8 suffix only while streaming;
   malformed bytes and unsupported format/emoji codepoints are visible escapes.
   CJK and base+combining sequences follow caller UTF-8 locale wcwidth. Every
   supported cluster stays together. Equal-share table widths replace the
   proposed content-sized heuristic for this first implementation: deterministic
   sizing needs no retained whole-table AST. Narrow tables use vertical records.
   Reflow is still triggered by geometry changes. This is an explicit design
   simplification, not a claim of content-adaptive column sizing.

Final pre-code review: public declarations cover lifecycle, failures, finite
viewport, phases and plain fallback. No synthetic source suffix is required:
parser EOF closes blocks and layout contributes provisional borders. Optional
inline completion remains deferred. Conservative per-block provisional borders
may appear on already closed code blocks while their message is streaming;
final phases remove all PREVIEW origin flags.

Worktree workflow exception: sibling worktree writes require interactive sandbox
approval and stalled 39 minutes. Orchestrator assigned disjoint paths in the
writable checkout; root owns git operations. Cog remains untouched.

Entities retain spelling rather than decode into characters; this both avoids an
extra entity database and makes terminal-control entity payloads inert. Inline
styles within table cells are currently flattened to cell text (header remains
bold); complete cell text and safe link destinations remain visible. This is a
first-version rendering limitation, recorded explicitly for follow-up.

## Cache-accounting contract addition (before implementation)

Integration requested `cog_tui_md_frame_storage(frame, bytes)` to bound the sum
of immutable cached frame allocations rather than charging the maximum 4MiB
for every small frame. Pass 1: returns owned allocation capacities including
frame/row/run/text storage, excluding allocator bookkeeping and parse scratch.
Pass 2: NULL-frame/output leaves output unchanged; frame remains independent of
context lifetime; tail-row eviction and ring rotation preserve exact accounting.
Pass 3: read-only, allocation-free, owner-thread semantics match frame_view;
result is stable and <= configured frame_bytes. No ownership or ABI layout of
existing public types changes. Final review found no missing scenario.

Implementation evidence: normal and Clang sanitizer contract/fuzz/allocation
tests pass against the pinned parser source. Independent critic R1 (interior
table cell mistaken for stream suffix) was fixed with a focused regression;
only the full authoritative source boundary may hold incomplete UTF-8.

Independent critic R2: cached the document pipe-column preflight once per render,
removing quadratic rescans for many tables. Contract semantics are unchanged;
4000-table regression verifies successful layout and total row count. Normal
contract/fuzz/allocation tests pass after the correction.
