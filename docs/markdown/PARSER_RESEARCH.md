# M1 parser evidence and selection

MD4C 0.5.2 is selected behind a private adapter. Public declarations and README
were inspected at the upstream release URL:
<https://github.com/mity/md4c/tree/release-0.5.2>.
It provides whole-document callbacks, not a persistent incremental parser.

## Executed probe

`tests/probes/markdown_probe.c` exercises every byte prefix of fenced code,
valid/partial tables, unmatched strong text, reference definitions, escaped pipes,
Unicode and table overflow. An installed `libmd4c.so.0.5.2` supplied initial
runtime evidence while sandbox network approval for vendoring was pending:

- An unclosed fence emits a code block at EOF; no synthetic closing syntax is
  necessary. The adapter adds a visual panel border only.
- Unmatched strong delimiters remain literal. Speculative inline closing syntax
  remains disabled.
- A table with two header columns and three body cells emits only four bytes of
  authored callback text in the fixture: its extra body cell is silently dropped.
  The adapter therefore preflights pipe-column counts and returns PROTOCOL for a
  raw-source fallback when any line may exceed the table column count.
- Some partial delimiter rows recognize a table with fewer columns temporarily.
  The same conservative fallback preserves all text until later bytes clarify
  the syntax. This is expected during prefix tests.
- Callback normalization means pointer ranges are not a public source-offset
  contract. Frames expose source-derived/decorative/preview origin categories,
  never fabricated exact source offsets.

The initial 128KiB all-asterisk parser probe was 0.059ms mean on this host; it is
mostly a horizontal-rule fast path and is not sufficient adversarial evidence.
The renderer fuzz benchmark also covers 128KiB alternating emphasis, repeated
incomplete links, and prose: initial optimized end-to-end means were 7.67ms,
8.96ms and 2.99ms. 1500 deterministic arbitrary-byte inputs averaged 0.107ms.
These are observations, not time/memory sandbox guarantees. ASan instrumentation
raises the heaviest case above 50ms; latency targets apply to normal builds.

The adapter and parser have independent resource boundaries. UI-owned storage,
source size, nesting, table columns and visible rows are bounded. MD4C internal
allocations are not controlled by the UI frame budget. A callback-bound failure
returns a normal error for a visibly identified plain fallback.

## Unicode and origin policy

Callers establish a UTF-8 LC_CTYPE locale. CJK widths and base-plus-combining
sequences use libc `wcwidth`. Emoji, variation selectors, bidi/format characters,
invalid bytes, terminal controls, and orphan combining marks are visible escapes.
Valid incomplete suffixes of up to three bytes are withheld only while STREAMING;
terminal outcomes display malformed final bytes. This policy is deterministic
within a locale and avoids terminal-specific emoji width assumptions.

`PREVIEW` is applied to code/table bottom borders during streaming, including
blocks that may already have closed earlier in the still-active message. Exact
COMPLETE/CANCELLED/FAILED rendering removes preview flags. Source is never
modified. No incomplete syntax is repaired in Cog or persisted anywhere.

## Validation status

Initial adapter contract/prefix/UTF-8/viewport/source-fidelity matrix and 309
adapter allocation-failure points passed with ASan/UBSan against the installed
parser. These initial runs do not instrument the system parser and do not count
as the final vendored-parser sanitizer gate. Final pin hashes and rerun evidence
will be recorded after dependency acquisition. LeakSanitizer requires the root
agent's escalated test invocation in this sandbox.

## Vendored-source gate (completed 2026-11-17)

The unmodified 0.5.2 sources and MIT license are now stored in `vendor/md4c`;
`vendor/md4c/README.md` records SHA-256 pins. The standalone probe was rerun
against that source (0.067ms all-asterisk parser mean, same prefix results).
Strict GCC and Clang builds compile every production header independently.
The vendored target is included in ASan/UBSan instrumentation.

All five new CTests (Markdown contracts, fuzz, allocation failures, screen
contracts and terminal-cell screen rendering) pass in normal and sanitizer
builds. Parser-inclusive allocation injection covers 314 failure points. The
normal CMake build (without an optimized build type) records approximately
0.172ms mean for 1500 arbitrary-byte inputs and 0.31/12.29/28.86/5.05ms for the
four 128KiB corpora. These differ from the explicit `-O2` system-parser baseline
above; neither measurement is a hard latency guarantee. The full application
integration and leak-enabled final release run remain separate root-agent gates.

Independent critic review found a cell-boundary bug: streaming table subframes
could hold malformed bytes at an interior cell end as though it were the source
suffix. Fixed by treating cell input as complete after the outer source suffix
has been handled. A regression verifies interior `A F0 9F | B` bytes visibly
escape and report zero pending suffix bytes. This prevents streaming-only source
loss without changing the authoritative-source phase contract.

Independent critic R2 found quadratic overflow preflight: each table rescanned
the entire source. A 52KiB document with 4000 one-column tables took 321.4ms in
the critic's optimized benchmark. The adapter now computes the maximum unescaped
pipe-cell count once per render and compares each table in constant time. The
same reproducer takes 29.6ms after the fix (`-O2`, this host), preserves all
16000 logical rows, and is included in the deterministic performance corpus.
This retains the conservative raw-fallback policy while removing the accidental
source-by-table-count factor.
