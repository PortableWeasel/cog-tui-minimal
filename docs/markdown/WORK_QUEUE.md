# Markdown implementation queue

Blueprint planning ticket: `a3f86e2`. The following tickets are open planned work;
none were implemented by this planning task. Dependencies are recorded in tix.
No automatic source changes or user approval gates are implied by this queue.

| Stage | Ticket | Work | Depends on |
|---|---|---|---|
| M1 | `bebe0fe` | Prefix/provenance/performance parser probe; select and pin parser | — |
| M2 | `e300aaf` | Readline/ncurses single-screen-owner PoC | — |
| M3 | `d22bba5` | Bounded pure Markdown preview and row layout | M1 |
| M4 | `5974282` | ANSI screen/input-view extraction | M2 |
| M5 | `d3b34a7` | Streaming reconciliation, previews and viewport cache | M3, M4 |
| M6 | `c385c8a` | Optional ncursesw backend or recorded evidence-based deferral | M2, M5 |
| M7 | `24ee93f` | Final review, sanitizer/performance/PTY/installation gates | M5, M6 |

Every implementation ticket includes architect contract review, implementation
and contract tests, critic review, integration and finalization. If a PoC changes
interface semantics, revise the draft headers and scenario traces before `.c`
implementation. Routine gates proceed autonomously under the repository rules.

Scope decisions: M1 selects a parser only with evidence. M2 may retain ncurses as
deferred if readline composition fails; M6 must then close with that finding.
Core Markdown remains backend-neutral and can ship on ANSI. Optional inline
syntax completion follows a separate provenance gate within M3/M5; literal
unmatched inline text is the initial fallback. No ticket requires Cog changes.
