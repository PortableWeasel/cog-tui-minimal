# Markdown implementation queue

Blueprint planning ticket: `a3f86e2`. Implementation has progressed through the
stages below using only installed public Cog interfaces. Tix holds lifecycle and
review history. No Cog source/configuration changes or rebuild are part of this
Markdown release.

| Stage | Ticket | Implementation state | Depends on |
|---|---|---|---|
| M1 | `bebe0fe` | Parser prefix/provenance/performance probe executed; MD4C 0.5.2 privately pinned with license/hashes | — |
| M2 | `e300aaf` | Hidden readline writes demonstrated; ANSI single-painter ownership selected; full ncurses gate remains unproved | — |
| M3 | `d22bba5` | Bounded Markdown/Unicode row layout, EOF previews, plain fallback and allocation/prefix tests implemented | M1 |
| M4 | `5974282` | ANSI scene/input screen implemented; contracts and libvterm cell tests pass | M2 |
| M5 | `d3b34a7` | Public-snapshot reconciliation, bounded cache, scroll anchors, readline bridge and paste ownership implemented; integration tests pass | M3, M4 |
| M6 | `c385c8a` | Evidence-based ncursesw deferral selected; backend request explicitly returns UNSUPPORTED | M2, M5 |
| M7 | `24ee93f` | Complete: independent critic approval, full normal and leak-enabled ASan/UBSan suites 20/20, UI-only install and installed PTY gate passed | M5, M6 |

Three header/scenario passes preceded implementation. Additional contract reviews
covered actual frame storage, screen/readline ownership, pure transcript identity,
fatal-prefix handling and bracketed paste before their corresponding changes.
Independent critic findings were corrected with regression tests, including
cross-role duplicate durable IDs and the hidden readline paste-mode writes.

Optional speculative inline completion remains deferred; unmatched inline syntax
stays literal. Code/table EOF previews use visual boundaries, never authored
synthetic bytes. MD4C/Unicode/performance limitations remain documented in
PARSER_RESEARCH.md and the production headers. Historical draft headers under
`docs/markdown/include/` are design artifacts; production headers are authoritative.

The full offline gate passed 20/20 normal tests and 20/20 Clang ASan/UBSan tests
with leak detection enabled. Separate live acceptance made four requests total:
two terminal-probe requests with inconclusive raw marker extraction, then two
public-snapshot requests with confirmed recall and clean cleanup. No live
cancellation request was sent. See `docs/VALIDATION.md` for exact evidence and
remaining acceptance limits. UI-only installation and final release signoff are
complete. Ncurses and speculative inline completion remain explicitly deferred.
