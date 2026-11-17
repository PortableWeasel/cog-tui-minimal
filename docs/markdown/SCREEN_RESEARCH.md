# M2 screen ownership probe and M4 contract review

The screen task uses disjoint files in the primary checkout because the sibling
worktree was read-only under the sandbox; its escalation was interrupted. Root
owns commits and ticket mutations. No Cog files are changed.

## Executed probe

`tests/screen_readline_probe.c` is a small standalone readline experiment. Build
with `cc -std=c11 -Wall -Wextra -Werror ... -lreadline`; drive under a PTY with
TERM=xterm-256color. A no-op custom redisplay callback and an owned temporary
`rl_outstream` capture expose writes outside redisplay. On this installed readline:

- accepting `hello` produced zero captured bytes;
- Ctrl-L produced one newline outside redisplay;
- completion of `/usr/` produced 303 bytes of directory listing;
- reverse-search/abort emitted audible bells outside the captured stream.

Therefore a redisplay hook alone does not establish single-writer ownership.
The application must route auxiliary output to a nonterminal sink, disable bell
style (saving/restoring it), and deliberately define completion/search behavior.
ANSI can repaint from authoritative rows after each input action. Optional
ncurses is deferred for this release: the full hidden-writer/keymap matrix and
newterm lifecycle have not been proved. This is a specific incomplete ownership
gate, not a claim that readline/ncurses integration is impossible. M6 should
close as the blueprint's evidence-based deferral after M5 validation. The frame
API remains suitable for a future ncurses backend; requesting it returns
UNSUPPORTED explicitly.

## Final header review before implementation

Three passes already exist in REVIEW.md. This additional implementation pass
resolves ownership: the application installs/restores readline hooks and stages
input, while screen owns physical output only. No screen global binding needed.
The screen never consumes stdin. open/close borrow explicit terminal streams.

Geometry comes from ioctl and tiny terminals are not replaced by 80x24 guesses.
Leave the final column unused to avoid autowrap; reserve separator/status and
input rows. A stale geometry yields AGAIN before painting. Input, labels and
status are independently sanitized; immutable Markdown runs are also validated
before any write. All text uses counted writes. Input is horizontally clipped
by display cells, with byte cursor mapped to a safe UTF-8 boundary. Markdown combining
marks retain their supported base cluster. Input combining marks, format
controls and emoji scalars are visibly escaped for deterministic cursor display. No input buffer or cursor mutation is permitted.

Validation will include screen PTY cell assertions, invalid/oversize contracts,
Unicode/control input, tiny geometry, stale geometry, teardown, and strict build.
EOF/async/readline integration remains M5, so module completion alone cannot
close the extracted-app acceptance gate.

## M4 module validation

`src/screen.c` and `include/cog_tui/screen.h` implement the ANSI scene backend.
The optional ncurses selector returns UNSUPPORTED. The application owns all
readline hooks/streams, terminal input modes and signal handlers. Screen open
requires terminal streams and a non-dumb TERM; streams are never closed.
Geometry reserves the final column to avoid autowrap and two input/status rows,
with a bounded output viewport of at most 300 by 1000 cells. Tiny terminals get
a clipped resize notice. Every scene is validated before any physical output;
resizes yield AGAIN. Physical IO errors may leave a partial repaint as contracted.

Executed standalone GCC strict C11 build and `tests/screen_test.c`: PASS.
Executed `tests/screen_cells_test.py` with libvterm: PASS. Cell checks cover role
labels, styled message placement, prompt separation, CJK width, visibly escaped
controls/combining marks, and a byte cursor inside a UTF-8 encoding. Contract
checks additionally cover unsupported backend, NULL/invalid/oversize input,
invalid and mismeasured runs, input-copy preservation after failure, long draft
clipping, resize/AGAIN, tiny geometry and screen restoration.

Clang ASan+UBSan tests pass with leak detection disabled. Leak-enabled execution
inside this sandbox fails at exit because LeakSanitizer cannot run under ptrace;
the final integration gate must rerun leak-enabled tests outside that restriction.
This is not a claimed leak check. CMake integration belongs to root; include
screen.c in tui, add header-screen self-containment, link screen-test with tui
and util, and register screen + screen-cells (Python, skip return 77).

Full readline keymap/async/exit validation remains the M5 integration gate.
